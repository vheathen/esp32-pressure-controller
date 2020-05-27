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

#include "adc_esp32.h"

static const char *TAG = "ADC";

/*
860 mV - 100 kPa

*/

typedef struct
{
    uint8_t channel;
} channel_config_t;

#define SENSOR_MAX_PRESSURE 1200000 // Pa

#define PRESSURE_HISTORY_TLS_INDEX 1
#define PRESSURE_HISTORY_VALUES_COUNT 5
#define PRESSURE_MEASURE_CYCLE_MS 40 // in miliseconds

#define DEFAULT_VREF 1100 //Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES 128 //Multisampling

static TaskHandle_t channel_tasks[CHAN_COUNT];

static const adc_channel_t reference_voltage_channel = ADC_CHANNEL_7; // GPIO35

#define REF_DIV_R1 1640
#define REF_DIV_R2 1430

static const double ref_voltage_div = (double)REF_DIV_R2 / (REF_DIV_R1 + REF_DIV_R2);

#define INPUT_DIV_R1 1130
#define INPUT_DIV_R2 2640

static const double input_voltage_div = (double)INPUT_DIV_R2 / (INPUT_DIV_R1 + INPUT_DIV_R2);

static esp_adc_cal_characteristics_t *adc_chars;

static const adc_bits_width_t width = ADC_WIDTH_BIT_12;
static const adc_atten_t atten = ADC_ATTEN_DB_11;
static const adc_unit_t unit = ADC_UNIT_1;

int32_t pressures[CHAN_COUNT] = {PRESSURE_SENSOR_ABSENT};

static TaskHandle_t uiTaskHandle;

#if 0
static void check_efuse(void)
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK)
    {
        printf("eFuse Two Point: Supported\n");
    }
    else
    {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK)
    {
        printf("eFuse Vref: Supported\n");
    }
    else
    {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    {
        printf("Characterized using Two Point Value\n");
    }
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        printf("Characterized using eFuse Vref\n");
    }
    else
    {
        printf("Characterized using Default Vref\n");
    }
}
#endif

uint32_t
measure_absolute_voltage(adc_channel_t channel)
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

int32_t calc_pressure(uint32_t voltage, uint32_t reference_voltage)
{
    // Min pressure = 0 Pa
    // Min pressure voltage = 10% of reference voltage
    // Max pressure = 1 200 000 Pa / 1200 kPa / 1.2 mPa
    // Max pressure voltage = 90% of reference voltage

    int32_t pressure = 0;

    // uint32_t actual_zero = 510;
    // double c = round((double)actual_zero / min_voltage * PRECISION) / PRECISION;
    double actual_c = 1.055900621;

    uint32_t min_voltage = round(reference_voltage * 0.1 * actual_c);
    uint32_t max_voltage = round(reference_voltage * 0.9 * actual_c);

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

void measure_channel_pressure_task(void *pvParameters)
{

    uint8_t pos = ((channel_config_t *)pvParameters)->channel;
    uint8_t channel = channels[((channel_config_t *)pvParameters)->channel];

    uint32_t reference_voltage, measured_voltage, actual_voltage;

    int32_t pressure;

    // pressures[pos] = PRESSURE_SENSOR_ABSENT;
    // xTaskNotify(uiTaskHandle, (1UL << 0), eSetBits);

    while (1)
    {
        xTaskNotifyWait(ULONG_MAX,          /* Reset the notification value to 0 on entry. */
                        ULONG_MAX,          /* Reset the notification value to 0 on exit. */
                        &reference_voltage, /* Notified value pass out in
                                              reference_voltage. */
                        portMAX_DELAY);     /* Block indefinitely. */

        measured_voltage = measure_absolute_voltage(channel);

        if (measured_voltage > 0)
        {
            actual_voltage = calc_actual_voltage(measured_voltage, input_voltage_div);
            pressure = calc_pressure(actual_voltage, reference_voltage);
        }
        else
        {
            actual_voltage = 0;
            pressure = PRESSURE_SENSOR_ABSENT;
        }

        if (pressures[pos] != pressure)
        {
            pressures[pos] = pressure;
            xTaskNotify(uiTaskHandle, (1UL << 0UL), eSetBits);
            ESP_LOGI(TAG, "\tCh: %d, MeasuredV: %04d mV, ActualV: %04d mV, RefV: %04d mV, Pressure: %06d Pa",
                     (int)channel, measured_voltage,
                     actual_voltage, reference_voltage,
                     pressure);
        }

        // ESP_LOGI(TAG, "Channel: %d, MeasuredV: %04d mV\t ActualV: %04d mV\t Pressure: %06d Pa", (int)channel, measured_voltage, actual_voltage, pressure);
    }
}

void measure_reference_voltage_task(void *pvParameters)
{
    {
        // TickType_t startTick, endTick, diffTick;

        //Continuously sample ADC1
        while (1)
        {
            // startTick = xTaskGetTickCount();

            uint32_t reference_voltage = measure_reference_voltage();

            for (int i = 0; i < CHAN_COUNT; i++)
            {
                if (channel_tasks[i] != NULL)
                {
                    xTaskNotify(channel_tasks[i], reference_voltage, eSetValueWithOverwrite);
                }
            }

            // endTick = xTaskGetTickCount();
            // diffTick = endTick - startTick;

            // ESP_LOGI(TAG, "Ref voltage: %d mV", reference_voltage);

            vTaskDelay(pdMS_TO_TICKS(PRESSURE_MEASURE_CYCLE_MS));
        }
    }
}

void measure_start(TaskHandle_t handle)
{
    uiTaskHandle = handle;

    measure_init();

    channel_config_t *ch_cfg;
    char name[12];

    for (int i = 0; i < CHAN_COUNT; i++)
    {
        ch_cfg = malloc(sizeof(channel_config_t));
        ch_cfg->channel = i;
        sprintf(name, "CH_%d", (int)(channels[i]));
        // ESP_LOGI(TAG, "Starting channel %d", (int)(ch_cfg->channel));
        xTaskCreate(measure_channel_pressure_task, name, 4096 * 2, ch_cfg, 2, &channel_tasks[i]);
    }

    xTaskCreate(measure_reference_voltage_task, "REF_VOLT", 4096 * 2, NULL, 2, NULL);
    // xTaskCreate(wait_for_reference_voltage_and_start_measure_task, "WAIT_FOR_REF_VOLTAGE", 4096 * 2, NULL, 2, NULL);
}

int32_t get_pressure(uint16_t index)
{
    return pressures[index];
}