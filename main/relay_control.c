#include "esp_event.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "pressure_sensors.h"
#include "relay.h"
#include "relay_control.h"

#include "utils.h"

static const char *TAG = "RELAY_CTRL";

#define MAX_ON_TIME_MS 5 * 60 * 1000  // 5 minutes in ms
#define MIN_OFF_TIME_MS 5 * 60 * 1000 // 5 minutes in ms

#define PRESSURE_SENSOR_INDEX 0
#define RELAY_INDEX 0

#define PRESSURE_LOW_MARK 250000  // pa
#define PRESSURE_HIGH_MARK 820000 // pa

enum relay_control_flags
{
  PRESSURE_UNDER_LOW_MARK  = 0x001,
  PRESSURE_ABOVE_HIGH_MARK = 0x002,
  MAX_ON_PERIOD_EXCEEDED   = 0x004,
  MIN_OFF_PERIOD_EXCEEDED  = 0x008,
  RELAY_IS_ON              = 0x010
};

typedef struct relay_controller
{
  EventGroupHandle_t event_group;
  uint8_t relay_index;
  uint8_t pressure_sensor_index;
  pressure_value_t pressure_low_mark;
  pressure_value_t pressure_high_mark;
  uint64_t max_on_time_ms;
  uint64_t min_off_time_ms;
  esp_timer_handle_t timer;
} Relay_controller_t;

/*
  Declarations
*/
void relay_control_task(void *pvParameter);

static void pressure_sensor_update_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

static void pressure_went_under_low_mark(Relay_controller_t *relay_controller);
static void pressure_went_above_low_mark(Relay_controller_t *relay_controller);
static void pressure_went_under_high_mark(Relay_controller_t *relay_controller);
static void pressure_went_above_high_mark(Relay_controller_t *relay_controller);

static void start_timer(const esp_timer_create_args_t *timer_args, esp_timer_handle_t *timer, uint64_t duration_ms);
static void reset_timer(Relay_controller_t *relay_controller);
static void reset_timer(Relay_controller_t *relay_controller);
static void start_on_timer(Relay_controller_t *relay_controller);
static void on_timer_finished(void *relay_controller);
static void start_off_timer(Relay_controller_t *relay_controller);
static void off_timer_finished(void *relay_controller);
static void turn_relay_on(Relay_controller_t *relay_controller);
static void turn_relay_off(Relay_controller_t *relay_controller);

void relay_control_start()
{
  xTaskCreate(relay_control_task, "relay ctrl", 4096 * 2, NULL, 0, NULL);
}

void relay_control_task(void *pvParameter)
{
  relays_init();

  Relay_controller_t relay_controller = {
      .relay_index           = RELAY_INDEX,
      .pressure_sensor_index = PRESSURE_SENSOR_INDEX,
      .pressure_low_mark     = PRESSURE_LOW_MARK,
      .pressure_high_mark    = PRESSURE_HIGH_MARK,
      .max_on_time_ms        = MAX_ON_TIME_MS,
      .min_off_time_ms       = MIN_OFF_TIME_MS,
      .timer                 = NULL};

  relay_controller.event_group = xEventGroupCreate();

  ESP_MEM_CHECK(TAG, relay_controller.event_group, abort());

  xEventGroupSetBits(relay_controller.event_group, MIN_OFF_PERIOD_EXCEEDED); // Assume it was OFF for enough time before start

  esp_event_handler_instance_register(PRESSURE_SENSORS_EVENTS, PRESSURE_SENSOR_VALUE_CHANGED, pressure_sensor_update_handler, &relay_controller, NULL);

  EventBits_t uxBits, lastBits = 0;

  ESP_LOGI(TAG, "Started");

  vTaskDelay(pdMS_TO_TICKS(2 * 1000));

  while (1)
  {
    uxBits = xEventGroupWaitBits(
        relay_controller.event_group,
        PRESSURE_UNDER_LOW_MARK | PRESSURE_ABOVE_HIGH_MARK | MAX_ON_PERIOD_EXCEEDED | MIN_OFF_PERIOD_EXCEEDED,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY);

    if (lastBits != uxBits)
    {
      ESP_LOGI(TAG, "Bits: %d", uxBits);
      lastBits = uxBits;
    }

    if ((uxBits & RELAY_IS_ON) != RELAY_IS_ON && // relay is OFF
        ((uxBits & (PRESSURE_UNDER_LOW_MARK | MIN_OFF_PERIOD_EXCEEDED)) == (PRESSURE_UNDER_LOW_MARK | MIN_OFF_PERIOD_EXCEEDED)))
    {
      ESP_LOGI(TAG, "Turning ON");
      turn_relay_on(&relay_controller);
    }

    if ((uxBits & RELAY_IS_ON) == RELAY_IS_ON && // relay is ON

        ((uxBits & MAX_ON_PERIOD_EXCEEDED) == MAX_ON_PERIOD_EXCEEDED ||

         ((uxBits & MIN_OFF_PERIOD_EXCEEDED) == MIN_OFF_PERIOD_EXCEEDED &&
          (uxBits & PRESSURE_UNDER_LOW_MARK) != PRESSURE_UNDER_LOW_MARK) ||

         (uxBits & PRESSURE_ABOVE_HIGH_MARK) == PRESSURE_ABOVE_HIGH_MARK))
    {
      ESP_LOGI(TAG, "Turning OFF");
      turn_relay_off(&relay_controller);
    }
  }
}

static void turn_relay_on(Relay_controller_t *relay_controller)
{
  xEventGroupClearBits(relay_controller->event_group, MAX_ON_PERIOD_EXCEEDED | MIN_OFF_PERIOD_EXCEEDED);
  relay_turn_on(relay_controller->relay_index);
  xEventGroupSetBits(relay_controller->event_group, RELAY_IS_ON);
  start_on_timer(relay_controller);
}

static void on_timer_finished(void *relay_controller)
{
  reset_timer(relay_controller);
  xEventGroupClearBits(((Relay_controller_t *)relay_controller)->event_group, MIN_OFF_PERIOD_EXCEEDED);
  xEventGroupSetBits(((Relay_controller_t *)relay_controller)->event_group, MAX_ON_PERIOD_EXCEEDED);
}

static void start_on_timer(Relay_controller_t *relay_controller)
{
  reset_timer(relay_controller);

  const esp_timer_create_args_t timer_args = {
      .callback = &on_timer_finished,
      .arg      = relay_controller,
      .name     = "ON time watchdog"};

  start_timer(&timer_args, &(relay_controller->timer), relay_controller->max_on_time_ms);
}

static void turn_relay_off(Relay_controller_t *relay_controller)
{
  xEventGroupClearBits(relay_controller->event_group, MAX_ON_PERIOD_EXCEEDED);
  relay_turn_off(relay_controller->relay_index);
  xEventGroupClearBits(relay_controller->event_group, RELAY_IS_ON);
  start_off_timer(relay_controller);
}

static void off_timer_finished(void *relay_controller)
{
  reset_timer(relay_controller);
  xEventGroupSetBits(((Relay_controller_t *)relay_controller)->event_group, MIN_OFF_PERIOD_EXCEEDED);
}

static void start_off_timer(Relay_controller_t *relay_controller)
{

  reset_timer(relay_controller);

  const esp_timer_create_args_t timer_args = {
      .callback = &off_timer_finished,
      .arg      = relay_controller,
      .name     = "ON time watchdog"};

  start_timer(&timer_args, &(relay_controller->timer), relay_controller->min_off_time_ms);
}

static void pressure_went_under_low_mark(Relay_controller_t *relay_controller)
{
  xEventGroupSetBits(relay_controller->event_group, PRESSURE_UNDER_LOW_MARK);
}

static void pressure_went_above_low_mark(Relay_controller_t *relay_controller)
{
  xEventGroupClearBits(relay_controller->event_group, PRESSURE_UNDER_LOW_MARK);
}

static void pressure_went_under_high_mark(Relay_controller_t *relay_controller)
{
  xEventGroupClearBits(relay_controller->event_group, PRESSURE_ABOVE_HIGH_MARK);
}

static void pressure_went_above_high_mark(Relay_controller_t *relay_controller)
{
  xEventGroupSetBits(relay_controller->event_group, PRESSURE_ABOVE_HIGH_MARK);
}

static void pressure_sensor_update_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  Relay_controller_t *relay_controller = (Relay_controller_t *)event_handler_arg;
  sensor_pressure_t *sensor            = (sensor_pressure_t *)event_data;

  if (sensor->index == relay_controller->pressure_sensor_index && sensor->pressure >= 0)
  {
    if (sensor->pressure < relay_controller->pressure_low_mark)
    {
      pressure_went_under_low_mark(relay_controller);
    }
    else
    {
      pressure_went_above_low_mark(relay_controller);
    }

    if (sensor->pressure > relay_controller->pressure_high_mark)
    {
      pressure_went_above_high_mark(relay_controller);
    }
    else
    {
      pressure_went_under_high_mark(relay_controller);
    }
  }
}

static void reset_timer(Relay_controller_t *relay_controller)
{
  esp_timer_handle_t control_timer = relay_controller->timer;
  if (control_timer != NULL)
  {
    esp_timer_stop(control_timer);
    esp_timer_delete(control_timer);
  }

  relay_controller->timer = NULL;
}

static void start_timer(const esp_timer_create_args_t *timer_args, esp_timer_handle_t *timer, uint64_t duration_ms)
{
  ESP_ERROR_CHECK(esp_timer_create(timer_args, timer));
  ESP_ERROR_CHECK(esp_timer_start_once(*timer, duration_ms * 1000)); // in microseconds
}