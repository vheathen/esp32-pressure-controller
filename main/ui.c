#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_system.h"

#include "pressure_sensors.h"
#include "ui.h"
#include "utils.h"

/* Littlevgl specific */
#include "lvgl/lvgl.h"
#include "lvgl_helpers.h"

/*
ST7789 Orientation:
270 deg:  {ST7789_MADCTL, {0x60}, 1},
0 deg: {ST7789_MADCTL, {0x0}, 1},
*/

/*********************
 *      DEFINES
 *********************/
// define ui events, check ui.h for implementation and changes
_UI_EVENTS(DEF_EVENT)

#define TAG "UI"
#define GUI_CPU_CORE 1
#define MAX_INTERACTION_TIME_MS 5000
#define PRESSURE_SENSOR_ABSENT_TEXT "-"
#define PRESSURE_SENSOR_OVERLOAD_TEXT "OVERLOAD"
#define PRESSURE_REFERENCE_POWER_ERROR_TEXT "RefV Err"

/**********************
 *      TYPEDEFS
 **********************/
typedef struct
{
  uint16_t index;
  int16_t value;
} gauge_data_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
void guiTask(void *pvParameter);

void gui_monitor_cb(lv_disp_drv_t *disp_drv, uint32_t time, uint32_t px);
void ui_init(void);

void create_gauge(lv_obj_t *gauges, uint16_t sensor_index);
void refresh_gauge(lv_obj_t *container);
void refresh_gauges();
void select_next_gauge();

static void pressure_sensor_update_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

void request_sensor_calibration();

void restart_interaction_timer();
void group_focus_cb(lv_group_t *group);
void stop_interaction_cb(void *arg);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_obj_t *gauges;
static lv_group_t *selection_group;
static lv_obj_t *hidden_selection           = NULL;
static esp_timer_handle_t interaction_timer = NULL;

static pressure_value_t sensor_pressure_values[] = {[0 ... SENSORS_COUNT] = PRESSURE_SENSOR_ABSENT};

//Creates a semaphore to handle concurrent call to lvgl stuff
//If you wish to call *any* lvgl function from other threads/tasks
//you should lock on the very same semaphore!
static SemaphoreHandle_t xGuiSemaphore;

/**********************
 *   APPLICATION MAIN
 **********************/
TaskHandle_t gui_start()
{
  TaskHandle_t uiTaskHandle;

  //If you want to use a task to create the graphic, you NEED to create a Pinned task
  //Otherwise there can be problem such as memory corruption and so on
  xTaskCreatePinnedToCore(guiTask, "gui", 4096 * 2, NULL, 0, &uiTaskHandle, GUI_CPU_CORE);

  return uiTaskHandle;
}

static void lv_tick_task(void *arg)
{
  (void)arg;

  lv_tick_inc(portTICK_RATE_MS);
}

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

  TaskHandle_t uiTaskHandle = xTaskGetCurrentTaskHandle();

  esp_event_handler_instance_register(PRESSURE_SENSORS_EVENTS, PRESSURE_SENSOR_VALUE_CHANGED, pressure_sensor_update_handler, uiTaskHandle, NULL);

  uint32_t ulNotifiedValue;

  while (1)
  {
    xTaskNotifyWait(0x00,              /*  Don't clear any notification bits on entry. */
                    ULONG_MAX,         /* Reset the notification value to 0 on exit. */
                    &ulNotifiedValue,  /* Notified value pass out in
                                              reference_voltage. */
                    pdMS_TO_TICKS(5)); /* Block for 5 ms. */

    if ((ulNotifiedValue & UI_PRESSURE_CHANGED) != 0)
    {
      refresh_gauges();
    }

    if ((ulNotifiedValue & UI_BUTTON_TAPPED) != 0)
    {
      select_next_gauge();
    }

    if ((ulNotifiedValue & UI_BUTTON_PUSHED) != 0)
    {
      restart_interaction_timer();
    }

    if ((ulNotifiedValue & UI_BUTTON_HELD_3_SEC) != 0)
    {
      request_sensor_calibration();
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
  // ESP_LOGI(TAG, "Group focus event");
  // if (lv_group_get_focused(group) == hidden_selection)
  //   ESP_LOGI(TAG, "Hidden obj got focused");
}

void ui_init(void)
{
  const esp_timer_create_args_t interaction_timer_args = {
      .callback = &stop_interaction_cb,
      .name     = "interaction_watchdog"};

  ESP_ERROR_CHECK(esp_timer_create(&interaction_timer_args, &interaction_timer));

  hidden_selection = lv_obj_create(lv_scr_act(), NULL);
  lv_obj_set_hidden(hidden_selection, true);
  // lv_obj_set_event_cb(hidden_selection, hidden_selection_event_cb);

  selection_group = lv_group_create();
  lv_group_add_obj(selection_group, hidden_selection);
  lv_group_set_focus_cb(selection_group, group_focus_cb);

  gauges = lv_cont_create(lv_scr_act(), NULL);

  lv_obj_set_style_local_pad_inner(gauges, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_DPX(9));
  lv_obj_set_style_local_pad_left(gauges, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_DPX(10));
  lv_obj_set_style_local_pad_right(gauges, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_DPX(0));
  lv_obj_set_style_local_pad_top(gauges, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, LV_DPX(9));

  // lv_obj_add_style(gauges, LV_CONT_PART_MAIN, &style);
  lv_cont_set_fit(gauges, LV_FIT_PARENT);
  lv_cont_set_layout(gauges, LV_LAYOUT_PRETTY_TOP);

  for (uint16_t i = 0; i < SENSORS_COUNT; i++)
  {
    create_gauge(gauges, i);
  }

  refresh_gauges();
}

void create_gauge(lv_obj_t *gauges, uint16_t sensor_index)
{
  gauge_data_t *gauge_data = calloc(sizeof(gauge_data_t), 1);
  gauge_data->index        = sensor_index;
  gauge_data->value        = INT16_MIN;

  lv_obj_t *container = lv_obj_create(gauges, NULL);
  lv_obj_set_size(container, 105, 71);

  lv_obj_set_user_data(container, gauge_data);

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
  lv_label_set_text(label, PRESSURE_SENSOR_ABSENT_TEXT);
  lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_align(label, LV_LABEL_ALIGN_CENTER);

  // static labels

  static lv_style_t label_style;

  lv_style_init(&label_style);
  lv_style_set_text_font(&label_style, LV_STATE_DEFAULT, lv_theme_get_font_small());
  lv_style_set_text_color(&label_style, LV_STATE_DEFAULT, lv_theme_get_color_primary());

  // index

  label = lv_label_create(container, NULL);
  lv_obj_add_style(label, LV_LABEL_PART_MAIN, &label_style);
  lv_label_set_text_fmt(label, "%d", sensor_index + 1);
  lv_obj_align(label, NULL, LV_ALIGN_IN_TOP_LEFT, LV_DPX(5), LV_DPX(3));

  // unit

  label = lv_label_create(container, NULL);
  lv_obj_add_style(label, LV_LABEL_PART_MAIN, &label_style);
  lv_label_set_static_text(label, "kPa");
  lv_obj_align(label, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -5);

  lv_group_add_obj(selection_group, container);
}

void refresh_gauge(lv_obj_t *container)
{
  gauge_data_t *data = (gauge_data_t *)lv_obj_get_user_data(container);
  int32_t value      = sensor_pressure_values[data->index];

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

    gauge      = child;
    gauge_text = lv_obj_get_child(gauge, NULL);

    switch (value)
    {
    case PRESSURE_REFERENCE_POWER_ERROR:
      lv_snprintf(text_value, sizeof(text_value), "%s", PRESSURE_REFERENCE_POWER_ERROR_TEXT);
      break;

    case PRESSURE_SENSOR_ABSENT:
      lv_snprintf(text_value, sizeof(text_value), "%s", PRESSURE_SENSOR_ABSENT_TEXT);
      break;

    case PRESSURE_SENSOR_OVERLOAD:
      lv_snprintf(text_value, sizeof(text_value), "%s", PRESSURE_SENSOR_OVERLOAD_TEXT);
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
  lv_obj_t *gauge = lv_obj_get_child(gauges, NULL);

  while (gauge)
  {
    refresh_gauge(gauge);
    gauge = lv_obj_get_child(gauges, gauge);
  };
}

void event_cb(lv_obj_t *obj, lv_event_t event)
{
  ESP_LOGI(TAG, "Event: %d", event);
}

void select_next_gauge()
{
  lv_group_focus_next(selection_group);
}

void stop_interaction_cb(void *arg)
{
  esp_timer_stop(interaction_timer);
  if (hidden_selection != NULL)
  {
    lv_group_focus_obj(hidden_selection);
  }
}

void restart_interaction_timer()
{
  esp_timer_stop(interaction_timer);
  ESP_ERROR_CHECK(esp_timer_start_once(interaction_timer, MAX_INTERACTION_TIME_MS * 1000)); // in microseconds
}

void request_sensor_calibration()
{
  lv_obj_t *selected = lv_group_get_focused(selection_group);

  if (selected != hidden_selection)
  {
    gauge_data_t *data = (gauge_data_t *)lv_obj_get_user_data(selected);
    calibrate_sensor(data->index);
  }
}

static void pressure_sensor_update_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

  TaskHandle_t uiTaskHandle = (TaskHandle_t)event_handler_arg;
  sensor_pressure_t *sensor = (sensor_pressure_t *)event_data;

  sensor_pressure_values[sensor->index] = sensor->pressure;

  xTaskNotify(uiTaskHandle, UI_PRESSURE_CHANGED, eSetBits);
}