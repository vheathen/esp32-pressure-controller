/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "esp_log.h"

#include <iot_button.h>

#include "button.h"

static const char *TAG = "BUTTON";

static TaskHandle_t uiTaskHandle;

static void push_btn_cb(void *arg)
{
  ESP_LOGI(TAG, "%s", (char *)arg);
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(uiTaskHandle, (1UL << 1UL), eSetBits, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
  {
    portYIELD_FROM_ISR();
  }
}

static void hold_btn_cb(void *arg)
{
  ESP_LOGI(TAG, "Hold");

  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(uiTaskHandle, (1UL << 2UL), eSetBits, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
  {
    portYIELD_FROM_ISR();
  }
}

void button_init(TaskHandle_t ui_handle)
{
  uiTaskHandle = ui_handle;

  button_handle_t btn_handle = iot_button_create(CONFIG_BUTTON_GPIO, CONFIG_BUTTON_ACTIVE_LEVEL);

  if (btn_handle)
  {
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, push_btn_cb, "RELEASE");
    iot_button_add_on_release_cb(btn_handle, 3, hold_btn_cb, NULL);
  }
}