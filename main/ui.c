
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

#include "math.h"

#include "st7789.h"
#include "fontx.h"

#include "adc_esp32.h"

static const char *TAG = "UI";

#define MAX_WIDTH CONFIG_WIDTH
#define MAX_HEIGHT CONFIG_HEIGHT
#define BORDER_LENGTH 10

#define BACKGROUND_COLOR BLACK
#define TEXT_COLOR YELLOW

FontxFile fx16G[2];
FontxFile fx32G[2];

TFT_t screen;

static int32_t current_pressure_values[CHAN_COUNT];

void drawText(TFT_t *dev, const char *ascii, FontxFile *fx, uint16_t x, uint16_t y, uint16_t color)
{

  uint8_t buffer[FontxGlyphBufSize];
  uint8_t fontWidth;
  uint8_t fontHeight;
  uint16_t xpos;
  uint16_t ypos;

  GetFontx(fx, 0, buffer, &fontWidth, &fontHeight);

  ypos = y - fontHeight / 2 - 1;
  xpos = x - strlen(ascii) * fontWidth / 2;
  lcdSetFontDirection(dev, DIRECTION0);

  lcdDrawString(dev, fx, xpos, ypos, (uint8_t *)ascii, color);
}

void drawCross(TFT_t *dev, uint16_t xc, uint16_t yc, uint16_t d, uint16_t color)
{

  uint16_t x0, x1, y0, y1, r;

  r = d / 2;

  x0 = xc < r ? 0 : xc - r;
  x1 = xc + r > MAX_WIDTH - 1 ? MAX_WIDTH - 1 : xc + r;
  y0 = yc < r ? 0 : yc - r;
  y1 = yc + r > MAX_HEIGHT - 1 ? MAX_HEIGHT - 1 : yc + r;

  lcdDrawLine(dev, x0, yc, x1, yc, color);
  lcdDrawLine(dev, xc, y0, xc, y1, color);
}

void init_ui()
{
  for (int i = 0; i < CHAN_COUNT; i++)
  {
    current_pressure_values[i] = INT32_MIN;
  }

  InitFontx(fx16G, "/spiffs/ILGH16XB.FNT", ""); // 8x16Dot Gothic
  InitFontx(fx32G, "/spiffs/ILGH32XB.FNT", ""); // 16x32Dot Gothic

  spi_master_init(&screen, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);
  lcdInit(&screen, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);

  // lcdSetFontFill(&screen, BACKGROUND_COLOR);

  lcdFillScreen(&screen, BACKGROUND_COLOR);

  for (uint16_t x = 0; x < MAX_WIDTH - 1; x += 119)
  {
    for (uint16_t y = 0; y < MAX_HEIGHT - 1; y += 79)
    {
      drawCross(&screen, x, y, BORDER_LENGTH * 2, WHITE);
    }
  }

  char label[4] = "x";

  for (uint8_t i = 1, y = 0; y < 4; y++)
  {
    for (uint8_t x = 0; x < 2; x++, i++)
    {
      if (i < 6)
      {
        sprintf(label, "%d", i);
        drawText(&screen, (char *)label, fx16G, x * 119 + 11, y * 79 + 29, RED);
      }
      else
      {
        drawText(&screen, "bar", fx32G, 180, 235, RED);
      }
    }
  }

  // drawText(&screen, "1", fx16G, 3, 3, RED);
  // drawText(&screen, "2", fx16G, 180, 75, RED);

  // drawText(&screen, "0.30", fx16G, 60, 155, RED);
  // drawText(&screen, "0.40", fx16G, 180, 155, RED);

  // drawText(&screen, "0.50", fx16G, 60, 235, RED);
  // drawText(&screen, "bar", fx16G, 180, 235, RED);

  //
  // drawText(&screen, "0.10", fx32G, 60, 75, RED);
  // drawText(&screen, "0.20", fx32G, 180, 75, RED);

  // drawText(&screen, "0.30", fx32G, 60, 155, RED);
  // drawText(&screen, "0.40", fx32G, 180, 155, RED);

  // drawText(&screen, "0.50", fx32G, 60, 235, RED);
}

char *value_to_text(int32_t value)
{
  char *text = calloc(sizeof(char), 20);

  switch (value)
  {
  case PRESSURE_REFERENCE_POWER_ERROR:
    sprintf(text, "RefVErr");
    break;

  case PRESSURE_SENSOR_ABSENT:
    sprintf(text, "-");
    break;

  case PRESSURE_SENSOR_OVERLOAD:
    sprintf(text, "!!!");
    break;

  default:
    sprintf(text, "%02.2f", (double)value / 100000); // Pa to Bar: 1 bar == 100 000 kPa
    break;
  }

  return text;
}

void update_value(uint8_t position, const char *old_value, const char *new_value)
{
  uint16_t ys[] = {75, 155, 235};
  uint16_t x, y;

  if (position % 2 == 0)
  {
    x = 180;
  }
  else
  {
    x = 60;
  };

  y = ys[(position - 1) / 2];

  drawText(&screen, old_value, fx32G, x, y, BACKGROUND_COLOR);
  drawText(&screen, new_value, fx32G, x, y, TEXT_COLOR);
}

void update_ui()
{
  char *old_value;
  char *new_value;

  for (int i = 0; i < CHAN_COUNT; i++)
  {
    if (pressures[i] != current_pressure_values[i])
    {
      old_value = value_to_text(current_pressure_values[i]);
      new_value = value_to_text(pressures[i]);

      current_pressure_values[i] = pressures[i];
      update_value(i + 1, old_value, new_value);

      free(old_value);
      free(new_value);
    }
  }
}

void select_next()
{
  ESP_LOGI(TAG, "Select next requested");
}

void calibrate_selected()
{
  ESP_LOGI(TAG, "Calibrate selected requested");
}

void ui_task(void *pvParameters)
{
  init_ui();

  update_ui();

  uint32_t ulNotifiedValue;

  while (1)
  {

    xTaskNotifyWait(0x00,             /*  Don't clear any notification bits on entry. */
                    ULONG_MAX,        /* Reset the notification value to 0 on exit. */
                    &ulNotifiedValue, /* Notified value pass out in
                                              reference_voltage. */
                    portMAX_DELAY);   /* Block indefinitely. */

    ESP_LOGI(TAG, "N: %d", ulNotifiedValue);

    if ((ulNotifiedValue & 0x01) != 0)
    {
      update_ui();
    }

    if ((ulNotifiedValue & 0x02) != 0)
    {
      select_next();
    }

    if ((ulNotifiedValue & 0x04) != 0)
    {
      calibrate_selected();
    }
  }
}
