/* ADC1 Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#include "pressure_sensors.h"
#include "ui.h"

static const char *TAG = "SENSORS";

/*
860 mV - 100 kPa

*/

typedef struct
{
    uint8_t index;
} channel_config_t;

// sensor task events
_SENSOR_EVENTS(DEF_EVENT)

#define SENSOR_MAX_PRESSURE 1200000 // Pa

#define SENSOR_MIN_PRESSURE_V_COEFF 0.1
#define SENSOR_MAX_PRESSURE_V_COEFF 0.9

#define PRESSURE_HISTORY_TLS_INDEX 1
#define PRESSURE_HISTORY_VALUES_COUNT 5
#define PRESSURE_MEASURE_CYCLE_MS 40 // in miliseconds

#define DEFAULT_VREF 1100 // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES 128 // Multisampling

static TaskHandle_t channel_tasks[CHAN_COUNT];

static const adc_channel_t channels[] = {
    ADC_CHANNEL_0, // GPIO36
    ADC_CHANNEL_3, // GPIO39
    ADC_CHANNEL_4, // GPIO32
    ADC_CHANNEL_5, // GPIO33
    ADC_CHANNEL_6  // GPIO34
};

static const adc_channel_t reference_voltage_channel = ADC_CHANNEL_7; // GPIO35

#define REF_DIV_R1 1640
#define REF_DIV_R2 1430

static const double ref_voltage_div = (double)REF_DIV_R2 / (REF_DIV_R1 + REF_DIV_R2);

static uint32_t reference_voltage;

#define INPUT_DIV_R1 1130
#define INPUT_DIV_R2 2640

static const double input_voltage_div = (double)INPUT_DIV_R2 / (INPUT_DIV_R1 + INPUT_DIV_R2);

static esp_adc_cal_characteristics_t *adc_chars;

static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;

static double channel_voltage_shift[CHAN_COUNT] = {[0 ... CHAN_COUNT - 1] = 1.0};

static int32_t pressures[CHAN_COUNT] = {PRESSURE_SENSOR_ABSENT};

static TaskHandle_t uiTaskHandle;

/**********************
 *  STATIC PROTOTYPES
 **********************/
uint32_t measure_absolute_voltage(adc_channel_t channel);
int32_t get_pressure(uint16_t index);
uint32_t calc_actual_voltage(uint32_t voltage, double div);
void freePressureHistory(int index, void *pressure_history);
void do_calibrate_channel(uint16_t index);
int32_t calc_pressure(uint16_t index, uint32_t voltage, uint32_t reference_voltage);
void measure_init();
uint32_t measure_reference_voltage();
void measure_channel_pressure(uint16_t index);
void measure_channel_pressure_task(void *pvParameters);
void measure_reference_voltage_task(void *pvParameters);
void measure_start(TaskHandle_t handle);

void measure_start(TaskHandle_t ui_handle)
{
    uiTaskHandle = ui_handle;

    measure_init();

    channel_config_t *ch_cfg;
    char name[12];

    for (int i = 0; i < CHAN_COUNT; i++)
    {
        ch_cfg = malloc(sizeof(channel_config_t));
        ch_cfg->index = i;
        sprintf(name, "CH_%d", (int)(channels[i]));
        // ESP_LOGI(TAG, "Starting channel %d", (int)(ch_cfg->channel));
        xTaskCreate(measure_channel_pressure_task, name, 4096 * 2, ch_cfg, 2, &channel_tasks[i]);
    }

    xTaskCreate(measure_reference_voltage_task, "REF_VOLT", 4096 * 2, NULL, 2, NULL);
    // xTaskCreate(wait_for_reference_voltage_and_start_measure_task, "WAIT_FOR_REF_VOLTAGE", 4096 * 2, NULL, 2, NULL);
}

uint32_t measure_absolute_voltage(adc_channel_t channel)
{

    uint32_t adc_reading = 0;

    //Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++)
    {
        if (unit == ADC_UNIT_1)
        {
            adc_reading += adc1_get_raw((adc1_channel_t)channel);
        }
        else
        {
            int raw;
            adc2_get_raw((adc2_channel_t)channel, width, &raw);
            adc_reading += raw;
        }
    }
    adc_reading /= NO_OF_SAMPLES;

    uint32_t voltage;

    //Convert adc_reading to voltage in mV
    if (adc_reading > 200)
    {
        voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    }
    else
    {
        voltage = 0;
    }

    return voltage;
}

uint32_t calc_actual_voltage(uint32_t voltage, double div)
{
    return (uint32_t)round(voltage / div / 10) * 10;
}

void freePressureHistory(int index, void *pressure_history)
{
    free(pressure_history);
}

void do_calibrate_channel(uint16_t index)
{
    adc_channel_t channel = channels[index];

    uint32_t ref_v = measure_reference_voltage();

    // uint32_t actual_zero = 510;
    // double c = round((double)actual_zero / min_voltage * PRECISION) / PRECISION;

    uint32_t measured_voltage = measure_absolute_voltage(channel);

    if (measured_voltage > 0)
    {
        uint32_t actual_min_v = calc_actual_voltage(measured_voltage, input_voltage_div);
        uint32_t expected_min_v = round(ref_v * SENSOR_MIN_PRESSURE_V_COEFF);
        double shift = (double)actual_min_v / expected_min_v;
        channel_voltage_shift[index] = shift;
    }
}

int32_t calc_pressure(uint16_t index, uint32_t voltage, uint32_t reference_voltage)
{
    // Min pressure = 0 Pa
    // Min pressure voltage = 10% of reference voltage
    // Max pressure = 1 200 000 Pa / 1200 kPa / 1.2 mPa
    // Max pressure voltage = 90% of reference voltage

    int32_t pressure = 0;

    double shift = channel_voltage_shift[index];

    // double actual_c = 1.055900621;

    uint32_t min_voltage = reference_voltage * SENSOR_MIN_PRESSURE_V_COEFF * shift;
    uint32_t max_voltage = reference_voltage * SENSOR_MAX_PRESSURE_V_COEFF * shift;

    // ESP_LOGI(TAG, "shift: %f, min_v: %d, max_v: %d", shift, min_voltage, max_voltage);

    if (voltage > max_voltage)
    {
        pressure = PRESSURE_SENSOR_OVERLOAD;
    }
    else
    {
        // pressure = (voltage < min_voltage) ? 0 : round((double)(voltage - min_voltage) / max_voltage * SENSOR_MAX_PRESSURE / 1000) * 1000;

        uint32_t *current_pressure_index, *pressure_history;
        // let's have current_pressure_index point to the first element of the array
        current_pressure_index = pressure_history = pvTaskGetThreadLocalStoragePointer(NULL, PRESSURE_HISTORY_TLS_INDEX);

        if (!pressure_history)
        {
            current_pressure_index = pressure_history = (uint32_t *)calloc(sizeof(uint32_t), PRESSURE_HISTORY_VALUES_COUNT + 1);
            *current_pressure_index = PRESSURE_HISTORY_VALUES_COUNT;

            vTaskSetThreadLocalStoragePointerAndDelCallback(NULL, PRESSURE_HISTORY_TLS_INDEX, pressure_history, freePressureHistory);
        }

        *current_pressure_index = ++(*current_pressure_index) < PRESSURE_HISTORY_VALUES_COUNT ? *current_pressure_index : 1;

        *(pressure_history + *current_pressure_index) = (voltage < min_voltage) ? 0 : round((double)(voltage - min_voltage) / max_voltage * SENSOR_MAX_PRESSURE / 1000) * 1000;

        for (int i = 1; i <= PRESSURE_HISTORY_VALUES_COUNT; i++)
        {
            pressure += *(pressure_history + i);
        }

        pressure = round(pressure / PRESSURE_HISTORY_VALUES_COUNT / 1000) * 1000;
    }

    return pressure;
}

void measure_init()
{
    //Check if Two Point or Vref are burned into eFuse
    // check_efuse();

    //Configure ADC
    adc1_config_width(width);

    for (int i = 0; i < CHAN_COUNT; i++)
    {
        adc1_config_channel_atten(channels[i], atten);
        channel_tasks[i] = NULL;
    }

    adc1_config_channel_atten(reference_voltage_channel, atten);

    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    // esp_adc_cal_value_t val_type =
    esp_adc_cal_characterize(unit, atten, width, DEFAULT_VREF, adc_chars);

    // print_char_val_type(val_type);
}

uint32_t measure_reference_voltage()
{
    uint32_t measured_voltage = measure_absolute_voltage(reference_voltage_channel);
    return calc_actual_voltage(measured_voltage, ref_voltage_div);
}

void measure_channel_pressure(uint16_t index)
{
    uint8_t channel = channels[index];

    uint32_t measured_voltage, actual_voltage, pressure;

    measured_voltage = measure_absolute_voltage(channel);

    if (measured_voltage > 0)
    {
        actual_voltage = calc_actual_voltage(measured_voltage, input_voltage_div);
        pressure = calc_pressure(index, actual_voltage, reference_voltage);
    }
    else
    {
        actual_voltage = 0;
        pressure = PRESSURE_SENSOR_ABSENT;
    }

    if (pressures[index] != pressure)
    {
        pressures[index] = pressure;
        xTaskNotify(uiTaskHandle, UI_PRESSURE_CHANGED, eSetBits);
        ESP_LOGI(TAG, "\tCh: %d, MeasuredV: %04d mV, ActualV: %04d mV, RefV: %04d mV, Pressure: %06d Pa",
                 (int)channel, measured_voltage,
                 actual_voltage, reference_voltage,
                 pressure);
    }
}

void measure_channel_pressure_task(void *pvParameters)
{
    uint16_t index = ((channel_config_t *)pvParameters)->index;
    free(pvParameters);

    uint32_t ulNotifiedValue;

    while (1)
    {
        xTaskNotifyWait(0x0,              /* Do not reset the notification value on entry. */
                        ULONG_MAX,        /* Reset the notification value to 0 on exit. */
                        &ulNotifiedValue, /* Notified value pass out in
                                              reference_voltage. */
                        portMAX_DELAY);   /* Block indefinitely. */

        if ((ulNotifiedValue & SENSOR_REF_V_MEASURED) != 0)
        {
            measure_channel_pressure(index);
        }

        if ((ulNotifiedValue & SENSOR_CALIBRATION_REQUESTED) != 0)
        {
            ESP_LOGI(TAG, "Got calibration notif");
            do_calibrate_channel(index);
        }
    }
}

void measure_reference_voltage_task(void *pvParameters)
{
    {
        while (1)
        {

            reference_voltage = measure_reference_voltage();

            for (int i = 0; i < CHAN_COUNT; i++)
            {
                if (channel_tasks[i] != NULL)
                    xTaskNotify(channel_tasks[i], SENSOR_REF_V_MEASURED, eSetBits);
            }

            vTaskDelay(pdMS_TO_TICKS(PRESSURE_MEASURE_CYCLE_MS));
        }
    }
}

int32_t get_pressure(uint16_t index)
{
    return pressures[index];
}

void calibrate_sensor(uint16_t index)
{
    if (channel_tasks[index] != NULL)
    {
        xTaskNotify(channel_tasks[index], SENSOR_CALIBRATION_REQUESTED, eSetBits);
    }
    else
    {
        ESP_LOGI(TAG, "Should calibrate sensor, but task handler is NULL. Index: %d", index);
    }
}