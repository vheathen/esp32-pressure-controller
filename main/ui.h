#ifndef _UI_H_
#define _UI_H_

#include "limits.h"

#define UI_INT_EVENT(event) int_##event,
#define UI_EVENT_EXTERN(event) extern const unsigned long event;
#define UI_EVENT(event) const unsigned long event = (1UL << int_##event);

#define _UI_EVENTS(EVENT)    \
  EVENT(UI_PRESSURE_CHANGED) \
  EVENT(UI_BUTTON_PUSHED)    \
  EVENT(UI_BUTTON_TAPPED)    \
  EVENT(UI_BUTTON_HELD_3_SEC)

enum UI_EVENTS
{
  _UI_EVENTS(UI_INT_EVENT)
      _BTN_EVENT_LAST = ULONG_MAX
};

_UI_EVENTS(UI_EVENT_EXTERN)

TaskHandle_t gui_start();

#endif // _UI_H_