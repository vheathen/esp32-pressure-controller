#ifndef _RELAY_H_
#define _RELAY_H_

#include "driver/gpio.h"
#include "esp_event.h"

#include "utils.h" // events declaration macroses etc

typedef enum
{
  RELAY_OFF = (uint8_t)0,
  RELAY_ON
} relay_state_t;

typedef struct relay
{
  gpio_num_t control_pin;
  relay_state_t state;
} relay_t;

ESP_EVENT_DECLARE_BASE(RELAYS_EVENTS);

#define _RELAYS_EVENTS(EVENT) \
  EVENT(RELAY_TURNED_ON)      \
  EVENT(RELAY_TURNED_OFF)

enum RELAY_EVENTS
{
  _RELAYS_EVENTS(DEF_INT_EVENT)
      _RELAYS_EVENT_LAST = ULONG_MAX
};

_RELAYS_EVENTS(DEF_EVENT_EXTERN)

void relays_init();
void relay_turn_on(uint8_t index);
void relay_turn_off(uint8_t index);

#endif // _RELAY_H_