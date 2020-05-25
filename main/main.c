#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "adc_esp32.h"
#include "ui.h"
#include "button.h"

static TaskHandle_t uiTaskHandle;

static const char *TAG = "MAIN";

void app_main(void)
{

	// xTaskCreate(ui_task, "UI", 1024 * 6, NULL, 5, &uiTaskHandle);

	measure_start(uiTaskHandle);
	button_init(uiTaskHandle);
}
