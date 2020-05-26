#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_freertos_hooks.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "adc_esp32.h"
#include "ui.h"

/* Littlevgl specific */
#include "lvgl/lvgl.h"
#include "lvgl_driver.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "demo"
#define GAUGE_COLS 2
#define MAX_INTERACTION_TIME_MS 5000

/**********************
 *      TYPEDEFS
 **********************/
typedef struct
{
  uint8_t index;
  int16_t value;
} gauge_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
void guiTask(void *pvParameter);

void gui_monitor_cb(lv_disp_drv_t *disp_drv, uint32_t time, uint32_t px);
void ui_init(void);

lv_obj_t *create_gauge(uint16_t row, uint16_t col);
void refresh_gauge(lv_obj_t *container, int16_t value);
void refresh_gauges();
void select_next_gauge();

void group_focus_cb(lv_group_t *group);
void stop_interaction_cb(void *arg);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t *gauges[CHAN_COUNT];
static lv_group_t *selection_group;
static lv_obj_t *hidden_selection;
static esp_timer_handle_t interaction_timer = NULL;

/**********************
 *   APPLICATION MAIN
 **********************/
TaskHandle_t gui_start()
{
  TaskHandle_t uiTaskHandle;

  //If you want to use a task to create the graphic, you NEED to create a Pinned task
  //Otherwise there can be problem such as memory corruption and so on
  xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 2, NULL, 0, &uiTaskHandle, 1);

  return uiTaskHandle;
}

static void lv_tick_task(void *arg)
{
  (void)arg;

  lv_tick_inc(portTICK_RATE_MS);
}

//Creates a semaphore to handle concurrent call to lvgl stuff
//If you wish to call *any* lvgl function from other threads/tasks
//you should lock on the very same semaphore!
SemaphoreHandle_t xGuiSemaphore;

void guiTask(void *pvParameter)
{
  (void)pvParameter;
  xGuiSemaphore = xSemaphoreCreateMutex();

  lv_init();

  lvgl_driver_init();

  static lv_color_t buf1[DISP_BUF_SIZE];
  static lv_color_t buf2[DISP_BUF_SIZE];
  static lv_disp_buf_t disp_buf;
  lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

  lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.flush_cb = disp_driver_flush;
  // disp_drv.monitor_cb = gui_monitor_cb;

  disp_drv.buffer = &disp_buf;
  lv_disp_drv_register(&disp_drv);

  const esp_timer_create_args_t periodic_timer_args = {
      .callback = &lv_tick_task,
      /* name is optional, but may help identify the timer when debugging */
      .name = "periodic_gui"};
  esp_timer_handle_t periodic_timer;
  ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
  //On ESP32 it's better to create a periodic task instead of esp_register_freertos_tick_hook
  ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10 * 1000)); //10ms (expressed as microseconds)

  // lv_demo_widgets();
  ui_init();

  uint32_t ulNotifiedValue;

  while (1)
  {
    xTaskNotifyWait(0x00,              /*  Don't clear any notification bits on entry. */
                    ULONG_MAX,         /* Reset the notification value to 0 on exit. */
                    &ulNotifiedValue,  /* Notified value pass out in
                                              reference_voltage. */
                    pdMS_TO_TICKS(5)); /* Block for 5 ms. */

    if (ulNotifiedValue != 0)
      ESP_LOGI(TAG, "N: %d", ulNotifiedValue);

    if ((ulNotifiedValue & 0x01) != 0)
    {
      refresh_gauges();
    }

    if ((ulNotifiedValue & 0x02) != 0)
    {
      select_next_gauge();
    }

    //Try to lock the semaphore, if success, call lvgl stuff
    if (xSemaphoreTake(xGuiSemaphore, (TickType_t)10) == pdTRUE)
    {
      lv_task_handler();
      xSemaphoreGive(xGuiSemaphore);
    }
  }

  //A task should NEVER return
  vTaskDelete(NULL);
}

void gui_monitor_cb(lv_disp_drv_t *disp_drv, uint32_t time, uint32_t px)
{
  ESP_LOGI(TAG, "%d px refreshed in %d ms", px, time);
}

void group_focus_cb(lv_group_t *group)
{
  ESP_LOGI(TAG, "Group focus event");
  if (lv_group_get_focused(group) == hidden_selection)
    ESP_LOGI(TAG, "Hidden obj got focused");
}

void ui_init(void)
{
  const esp_timer_create_args_t interaction_timer_args = {
      .callback = &stop_interaction_cb,
      .name = "interaction_watchdog"};

  ESP_ERROR_CHECK(esp_timer_create(&interaction_timer_args, &interaction_timer));

  hidden_selection = lv_obj_create(lv_scr_act(), NULL);
  lv_obj_set_hidden(hidden_selection, true);
  // lv_obj_set_event_cb(hidden_selection, hidden_selection_event_cb);

  selection_group = lv_group_create();
  lv_group_add_obj(selection_group, hidden_selection);
  lv_group_set_focus_cb(selection_group, group_focus_cb);

  for (int col = 1; col <= GAUGE_COLS; col++)
  {
    for (int row = 1; (row - 1) * GAUGE_COLS + col <= CHAN_COUNT; row++)
    {
      gauges[(row - 1) * GAUGE_COLS + col - 1] = create_gauge(row, col);
    }
  }
}

lv_obj_t *create_gauge(uint16_t row, uint16_t col)
{
  uint32_t index = (row - 1) * GAUGE_COLS + col;

  gauge_data_t gauge_data;
  gauge_data.index = index;
  gauge_data.value = INT16_MIN;

  lv_obj_t *container = lv_obj_create(lv_scr_act(), NULL);

  lv_obj_set_user_data(container, &gauge_data);

  lv_obj_set_size(container, 105, 71);
  lv_obj_set_pos(container, (col - 1) * 115 + 10, (row - 1) * 77 + 6);

  // gauge

  static lv_style_t gauge_style;
  lv_style_init(&gauge_style);

  lv_style_set_border_width(&gauge_style, LV_STATE_DEFAULT, 0);
  lv_style_set_radius(&gauge_style, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
  lv_style_set_pad_left(&gauge_style, LV_STATE_DEFAULT, 0);
  lv_style_set_pad_right(&gauge_style, LV_STATE_DEFAULT, 0);
  lv_style_set_pad_top(&gauge_style, LV_STATE_DEFAULT, LV_DPX(3));
  lv_style_set_pad_inner(&gauge_style, LV_STATE_DEFAULT, LV_DPX(25));
  lv_style_set_scale_width(&gauge_style, LV_STATE_DEFAULT, LV_DPX(10));

  lv_style_set_line_color(&gauge_style, LV_STATE_DEFAULT, lv_theme_get_color_primary());
  lv_style_set_scale_grad_color(&gauge_style, LV_STATE_DEFAULT, lv_theme_get_color_primary());
  lv_style_set_scale_end_color(&gauge_style, LV_STATE_DEFAULT, lv_color_hex3(0x888));
  lv_style_set_line_width(&gauge_style, LV_STATE_DEFAULT, LV_DPX(2));
  lv_style_set_scale_end_line_width(&gauge_style, LV_STATE_DEFAULT, LV_DPX(2));

  lv_obj_t *gauge = lv_linemeter_create(container, NULL);
  lv_obj_add_style(gauge, LV_LABEL_PART_MAIN, &gauge_style);

  lv_obj_set_size(gauge, 65, 65);
  lv_obj_align(gauge, NULL, LV_ALIGN_CENTER, 0, 0);

  lv_linemeter_set_range(gauge, 0, 800);
  lv_linemeter_set_scale(gauge, 240, 23);

  // labels

  lv_obj_t *label;

  // value label

  label = lv_label_create(gauge, NULL);

  // static labels

  static lv_style_t label_style;

  lv_style_init(&label_style);
  lv_style_set_text_font(&label_style, LV_STATE_DEFAULT, lv_theme_get_font_small());
  lv_style_set_text_color(&label_style, LV_STATE_DEFAULT, lv_theme_get_color_primary());

  // index

  label = lv_label_create(container, NULL);
  lv_obj_add_style(label, LV_LABEL_PART_MAIN, &label_style);
  lv_label_set_text_fmt(label, "%d", index);
  lv_obj_align(label, NULL, LV_ALIGN_IN_TOP_LEFT, LV_DPX(5), LV_DPX(3));

  // unit

  label = lv_label_create(container, NULL);
  lv_obj_add_style(label, LV_LABEL_PART_MAIN, &label_style);
  lv_label_set_static_text(label, "kPa");
  lv_obj_align(label, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -5);

  refresh_gauge(container, PRESSURE_SENSOR_ABSENT);

  lv_group_add_obj(selection_group, container);

  return container;
}

void refresh_gauge(lv_obj_t *container, int16_t value)
{
  gauge_data_t *data = (gauge_data_t *)lv_obj_get_user_data_ptr(container);

  if (data->value != value)
  {
    data->value = value;

    char text_value[20];
    int32_t gauge_value = 0;

    lv_obj_t *gauge, *gauge_text;
    lv_obj_type_t type;

    lv_obj_t *child = lv_obj_get_child(container, NULL);
    lv_obj_get_type(child, &type);

    while (child && strcmp(type.type[0], "lv_linemeter") != 0)
    {
      child = lv_obj_get_child(container, child);
      lv_obj_get_type(child, &type);
    };

    gauge = child;
    gauge_text = lv_obj_get_child(gauge, NULL);

    switch (value)
    {
    case PRESSURE_REFERENCE_POWER_ERROR:
      lv_snprintf(text_value, sizeof(text_value), "%s", "RefV Err");
      break;

    case PRESSURE_SENSOR_ABSENT:
      lv_snprintf(text_value, sizeof(text_value), "%s", "-");
      break;

    case PRESSURE_SENSOR_OVERLOAD:
      lv_snprintf(text_value, sizeof(text_value), "%s", "OVERLOAD");
      gauge_value = lv_linemeter_get_max_value(gauge);
      break;

    default:
      gauge_value = value / 1000; // Pa to Kpa
      lv_snprintf(text_value, sizeof(text_value), "%03d", gauge_value);
      break;
    }

    lv_linemeter_set_value(gauge, gauge_value);
    lv_label_set_text(gauge_text, text_value);
    lv_obj_align(gauge_text, NULL, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_align(gauge_text, LV_LABEL_ALIGN_CENTER);
  }
}

void refresh_gauges()
{
  for (int i = 0; i < CHAN_COUNT; i++)
  {
    refresh_gauge(gauges[i], pressures[i]);
  }
}

void event_cb(lv_obj_t *obj, lv_event_t event)
{
  ESP_LOGI(TAG, "Event: %d", event);
}

void select_next_gauge()
{
  esp_timer_stop(interaction_timer);
  ESP_ERROR_CHECK(esp_timer_start_once(interaction_timer, MAX_INTERACTION_TIME_MS * 1000)); // in microseconds

  lv_group_focus_next(selection_group);
}

void stop_interaction_cb(void *arg)
{
  esp_timer_stop(interaction_timer);
  lv_group_focus_obj(hidden_selection);
}