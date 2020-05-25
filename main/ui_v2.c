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

#include "ui.h"

#include "adc_esp32.h"

static const char *TAG = "UI2";

#define MAX_WIDTH CONFIG_WIDTH
#define MAX_HEIGHT CONFIG_HEIGHT

#define TILE_COLUMNS 2
#define TILE_ROWS 3

#define TILE_WIDTH MAX_WIDTH / TILE_COLUMNS
#define TILE_HEIGHT MAX_HEIGHT / TILE_ROWS

#define TILE_BACKGROUND PRIMARY_COLOR
#define TILE_SELECTED_BACKGROUND PRIMARY_DARK_COLOR
#define TILE_LABEL_COLOR DIM_TEXT_COLOR
#define TILE_TEXT_COLOR BRIGHT_TEXT_COLOR

FontxFile fx16G[2];
FontxFile fx32G[2];

typedef struct
{
  uint16_t index;
  uint16_t x;
  uint16_t y;
  bool selected;
  char *label;
  int32_t value;
  bool _prev_selected;
  int32_t _prev_value;
} tile_t;

TFT_t screen;

static tile_t *tiles;

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

void redrawTile(TFT_t *dev, uint16_t index)
{
}

void init_ui()
{
  InitFontx(fx16G, "/spiffs/ILGH16XB.FNT", ""); // 8x16Dot Gothic
  InitFontx(fx32G, "/spiffs/ILGH32XB.FNT", ""); // 16x32Dot Gothic

  spi_master_init(&screen, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);
  lcdInit(&screen, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);

  // lcdSetFontFill(&screen, BACKGROUND_COLOR);

  lcdFillScreen(&screen, LIGHT_BACKGROUND_COLOR);

  tile_t *tile;
  tile = tiles = calloc(sizeof(tile_t), CHAN_COUNT);

  for (int i = 0; i < CHAN_COUNT; i++, tile++)
  {
    tile->index = i;
    current_pressure_values[i] = INT32_MIN;
  }

  // for (uint16_t x = 0; x < MAX_WIDTH - 1; x += 119)
  // {
  //   for (uint16_t y = 0; y < MAX_HEIGHT - 1; y += 79)
  //   {
  //     drawCross(&screen, x, y, BORDER_LENGTH * 2, WHITE);
  //   }
  // }

  // char label[4] = "x";

  // for (uint8_t i = 1, y = 0; y < 4; y++)
  // {
  //   for (uint8_t x = 0; x < 2; x++, i++)
  //   {
  //     if (i < 6)
  //     {
  //       sprintf(label, "%d", i);
  //       drawText(&screen, (char *)label, fx16G, x * 119 + 11, y * 79 + 29, RED);
  //     }
  //     else
  //     {
  //       drawText(&screen, "bar", fx32G, 180, 235, RED);
  //     }
  //   }
  // }
}