#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"

#include "esp_event.h"
#include <sys/time.h>
#include <time.h>

#include "button.h"
#include "pressure_sensors.h"
#include "relay_control.h"
#include "ui.h"
#include "wifi.h"

static TaskHandle_t uiTaskHandle;

static const char *TAG = "MAIN";

#define SEC_PER_DAY 86400
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

void stats_task(void *arg);
void nvs_init();

void app_main(void)
{

  // const esp_timer_create_args_t stats_timer_args = {
  // 		.callback = &stats_task,
  // 		/* name is optional, but may help identify the timer when debugging */
  // 		.name = "stats"};
  // esp_timer_handle_t stats_timer;
  // ESP_ERROR_CHECK(esp_timer_create(&stats_timer_args, &stats_timer));
  // //On ESP32 it's better to create a periodic task instead of esp_register_freertos_tick_hook
  // ESP_ERROR_CHECK(esp_timer_start_periodic(stats_timer, 10000 * 1000)); //1000ms (expressed as microseconds)

  ESP_ERROR_CHECK(esp_event_loop_create_default());

  nvs_init();

  wifi_init();

  uiTaskHandle = gui_start();
  // button_init(uiTaskHandle);

  relay_control_start();
  measure_start();
}

void nvs_init()
{
  // Initialize NVS
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(err);
}

void stats_task(void *arg)
{
  static struct timeval start_tv = {-1, 0};
  struct timeval tv;
  struct timezone tz;

  gettimeofday(&tv, &tz);

  if (start_tv.tv_sec == -1)
  {
    start_tv = tv;
  }

  ESP_LOGI(TAG, "[APP] Time went, min: %ld\t Free memory: %d bytes", (tv.tv_sec - start_tv.tv_sec) / SEC_PER_MIN, esp_get_free_heap_size());
}
