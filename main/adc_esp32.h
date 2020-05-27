#ifndef _ADC_ESP32_H_
#define _ADC_ESP32_H_

#include <limits.h>
#include "driver/adc.h"

enum pressure_sensor_states
{
  PRESSURE_REFERENCE_POWER_ERROR = INT8_MIN,
  PRESSURE_SENSOR_ABSENT,
  PRESSURE_SENSOR_OVERLOAD
};

#define CHAN_COUNT 5

static const adc_channel_t channels[] = {
    ADC_CHANNEL_0, // GPIO36
    ADC_CHANNEL_3, // GPIO39
    ADC_CHANNEL_4, // GPIO32
    ADC_CHANNEL_5, // GPIO33
    ADC_CHANNEL_6  // GPIO34
};

void measure_start(TaskHandle_t handle);

int32_t get_pressure(uint16_t index);

#endif // _ADC_ESP32_H_