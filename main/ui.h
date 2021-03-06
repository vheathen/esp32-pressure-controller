#ifndef _UI_H_
#define _UI_H_

#include "limits.h"
#include "utils.h"

#define _UI_EVENTS(EVENT)    \
  EVENT(UI_PRESSURE_CHANGED) \
  EVENT(UI_BUTTON_PUSHED)    \
  EVENT(UI_BUTTON_TAPPED)    \
  EVENT(UI_BUTTON_HELD_3_SEC)

enum UI_EVENTS
{
  _UI_EVENTS(DEF_INT_EVENT)
      _UI_EVENT_LAST = ULONG_MAX
};

_UI_EVENTS(DEF_EVENT_EXTERN)

TaskHandle_t gui_start();

#endif // _UI_H_