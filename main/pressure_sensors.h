#ifndef _PRESSURE_SENSORS_H_
#define _PRESSURE_SENSORS_H_

#include <limits.h>
#include "driver/adc.h"

#include "utils.h"

enum pressure_sensor_states
{
  PRESSURE_REFERENCE_POWER_ERROR = INT8_MIN,
  PRESSURE_SENSOR_ABSENT,
  PRESSURE_SENSOR_OVERLOAD
};

#define CHAN_COUNT 5

#define _SENSOR_EVENTS(EVENT)  \
  EVENT(SENSOR_REF_V_MEASURED) \
  EVENT(SENSOR_CALIBRATION_REQUESTED)

enum SENSOR_EVENTS
{
  _SENSOR_EVENTS(DEF_INT_EVENT)
      _SENSOR_EVENT_LAST = ULONG_MAX
};

_SENSOR_EVENTS(DEF_EVENT_EXTERN)

void measure_start(TaskHandle_t handle);

int32_t get_pressure(uint16_t index);
void calibrate_sensor(uint16_t index);

#endif // _PRESSURE_SENSORS_H_