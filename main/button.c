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

#include "ui.h"
#include "button.h"

static const char *TAG = "BUTTON";

static TaskHandle_t uiTaskHandle;

static void tap_btn_cb(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(uiTaskHandle, UI_BUTTON_TAPPED, eSetBits, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
  {
    portYIELD_FROM_ISR();
  }
}

static void hold_btn_cb(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(uiTaskHandle, UI_BUTTON_HELD_3_SEC, eSetBits, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken)
  {
    portYIELD_FROM_ISR();
  }
}

static void push_btn_cb(void *arg)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xTaskNotifyFromISR(uiTaskHandle, UI_BUTTON_PUSHED, eSetBits, &xHigherPriorityTaskWoken);
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
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_PUSH, push_btn_cb, "TAP");
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_TAP, tap_btn_cb, "TAP");
    iot_button_add_on_press_cb(btn_handle, 3, hold_btn_cb, "HOLD");
  }
}