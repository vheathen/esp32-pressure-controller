/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_event.h"

#include "relay.h"

#define RELAY_1_PIN CONFIG_SOCKET_1_CONTROL_PIN
#define RELAY_2_PIN CONFIG_SOCKET_2_CONTROL_PIN
#define RELAYS_COUNT 2

ESP_EVENT_DEFINE_BASE(RELAYS_EVENTS);

static relay_t relays[] = {
    {.control_pin = RELAY_1_PIN, .state = RELAY_OFF},
    {.control_pin = RELAY_2_PIN, .state = RELAY_OFF}};

void relay_turn_on(uint8_t index)
{
  gpio_set_level(relays[index].control_pin, RELAY_ON);
  esp_event_post(RELAYS_EVENTS, RELAY_ON, &index, sizeof(uint8_t), portMAX_DELAY);
}

void relay_turn_off(uint8_t index)
{
  gpio_set_level(relays[index].control_pin, RELAY_OFF);
  esp_event_post(RELAYS_EVENTS, RELAY_OFF, &index, sizeof(uint8_t), portMAX_DELAY);
}

void relays_init()
{
  /* Configure output */
  gpio_config_t io_conf = {
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = 1,
  };
  io_conf.pin_bit_mask = 0x0;

  for (uint8_t i = 0; i < RELAYS_COUNT; i++)
  {
    io_conf.pin_bit_mask |= ((uint64_t)1 << (relays[i]).control_pin);
  }
  /* Configure the GPIO */
  gpio_config(&io_conf);
}
