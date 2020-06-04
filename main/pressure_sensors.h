#ifndef _PRESSURE_SENSORS_H_
#define _PRESSURE_SENSORS_H_

#include <limits.h>
#include "driver/adc.h"
#include "esp_event.h"

#include "utils.h" // events declaration macroses etc

ESP_EVENT_DECLARE_BASE(PRESSURE_SENSORS_EVENTS); // declaration of the pressure sensors events family

typedef int32_t pressure_value_t;

typedef struct sensor_pressure
{
  uint8_t index;
  pressure_value_t pressure;
} sensor_pressure_t;

enum pressure_sensor_states
{
  PRESSURE_REFERENCE_POWER_ERROR = INT8_MIN,
  PRESSURE_SENSOR_ABSENT,
  PRESSURE_SENSOR_OVERLOAD
};

#define SENSORS_COUNT 5

#define _PRESSURE_SENSORS_EVENTS(EVENT) \
  EVENT(PRESSURE_SENSOR_REF_V_MEASURED) \
  EVENT(PRESSURE_SENSOR_VALUE_CHANGED)  \
  EVENT(PRESSURE_SENSOR_CALIBRATION_REQUESTED)

enum SENSOR_EVENTS
{
  _PRESSURE_SENSORS_EVENTS(DEF_INT_EVENT)
      _PRESSURE_SENSORS_EVENT_LAST = ULONG_MAX
};

_PRESSURE_SENSORS_EVENTS(DEF_EVENT_EXTERN)

void measure_start();

pressure_value_t get_pressure(uint8_t index);
void calibrate_sensor(uint8_t index);

#endif // _PRESSURE_SENSORS_H_