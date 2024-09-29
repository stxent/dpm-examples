/*
 * helpers/display_helpers.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "display_helpers.h"
#include <dpm/displays/display.h>
#include <xcore/memory.h>
/*----------------------------------------------------------------------------*/
#define COLORS_TOTAL 7
/*----------------------------------------------------------------------------*/
Color interpolateColor(Color a, Color b, int current, int total)
{
  const Color result = {
      .r = a.r + (b.r - a.r) * current / total,
      .g = a.g + (b.g - a.g) * current / total,
      .b = a.b + (b.b - a.b) * current / total
  };
  return result;
}
/*----------------------------------------------------------------------------*/
Color makeColor(unsigned int color)
{
  static const Color colorTable[] = {
      {255, 255, 255}, /* White */
      {255, 0, 0},     /* Red */
      {255, 255, 0},   /* Yellow */
      {0, 255, 0},     /* Green */
      {0, 255, 255},   /* Cyan */
      {0, 0, 255},     /* Blue */
      {255, 0, 255}    /* Magenta */
  };
  const size_t index = (size_t)color % ARRAY_SIZE(colorTable);

  return colorTable[index];
}
/*----------------------------------------------------------------------------*/
uint16_t rgbTo565(Color color)
{
  const uint8_t r = color.r * 32 / 256;
  const uint8_t g = color.g * 64 / 256;
  const uint8_t b = color.b * 32 / 256;

  return toBigEndian16((r << 11) | (g << 5) | b);
}
/*----------------------------------------------------------------------------*/
void handleChessFill(struct Interface *display, unsigned int color,
    unsigned int style, void *buffer, size_t)
{
  const uint16_t colorA = style & 1 ?
      rgbTo565(makeColor(color)) : rgbTo565((Color){0, 0, 0});
  const uint16_t colorB = style & 1 ?
      rgbTo565((Color){0, 0, 0}) : rgbTo565(makeColor(color));
  uint16_t * const arena = buffer;
  struct DisplayResolution resolution;

  ifGetParam(display, IF_DISPLAY_RESOLUTION, &resolution);

  const uint16_t div = resolution.width / 8;

  for (uint16_t y = 0; y < resolution.height; ++y)
  {
    if ((y / div) & 1)
    {
      for (uint16_t x = 0; x < resolution.width; ++x)
        arena[x] = ((x / div) & 1) ? colorA : colorB;
    }
    else
    {
      for (uint16_t x = 0; x < resolution.width; ++x)
        arena[x] = ((x / div) & 1) ? colorB : colorA;
    }

    ifWrite(display, arena, resolution.width * sizeof(uint16_t));
  }
}
/*----------------------------------------------------------------------------*/
void handleGradientFill(struct Interface *display, unsigned int color,
    unsigned int style, void *buffer, size_t)
{
  const Color colorA = style & 1 ? makeColor(color) : (Color){0, 0, 0};
  const Color colorB = style & 1 ? (Color){0, 0, 0} : makeColor(color);
  uint16_t * const arena = buffer;
  struct DisplayResolution resolution;

  ifGetParam(display, IF_DISPLAY_RESOLUTION, &resolution);

  for (uint16_t y = 0; y < resolution.height; ++y)
  {
    const uint16_t value =
        rgbTo565(interpolateColor(colorA, colorB, y, resolution.height));

    for (uint16_t x = 0; x < resolution.width; ++x)
      arena[x] = value;

    ifWrite(display, arena, resolution.width * sizeof(uint16_t));
  }
}
/*----------------------------------------------------------------------------*/
void handleLineFill(struct Interface *display, unsigned int color,
    unsigned int style, void *buffer, size_t size)
{
  const unsigned int index = style % 3;
  const uint16_t colorA = rgbTo565(makeColor(color));
  const uint16_t colorB = rgbTo565((Color){0, 0, 0});
  uint16_t * const arena = buffer;
  struct DisplayResolution resolution;

  ifGetParam(display, IF_DISPLAY_RESOLUTION, &resolution);

  const uint16_t lines = (size / resolution.width) & ~1;

  switch (index)
  {
    case 1:
      for (size_t i = 0; i < (size_t)lines * resolution.width; ++i)
        arena[i] = (i & 1) ? colorA : colorB;
      break;

    case 2:
      for (uint16_t y = 0; y < lines; ++y)
      {
        for (uint16_t x = 0; x < resolution.width; ++x)
        {
          if (y & 1)
            arena[y * resolution.width + x] = colorA;
          else
            arena[y * resolution.width + x] = colorB;
        }
      }
      break;

    default:
      for (uint16_t y = 0; y < lines; ++y)
      {
        for (uint16_t x = 0; x < resolution.width; ++x)
        {
          if (y & 1)
            arena[y * resolution.width + x] = (x & 1) ? colorA : colorB;
          else
            arena[y * resolution.width + x] = (x & 1) ? colorB : colorA;
        }
      }
      break;
  }

  for (uint16_t row = 0; row < resolution.height;)
  {
    const size_t count = MIN(lines, resolution.height - row);

    ifWrite(display, arena, count * resolution.width * sizeof(uint16_t));
    row += count;
  }
}
/*----------------------------------------------------------------------------*/
void handleMarkerFill(struct Interface *display, unsigned int color,
    unsigned int style, void *buffer, size_t)
{
  const uint16_t colorA = style & 1 ?
      rgbTo565((Color){0, 0, 0}) : rgbTo565(makeColor(color));
  const uint16_t colorB = style & 1 ?
      rgbTo565(makeColor(color)) : rgbTo565((Color){0, 0, 0});
  uint16_t * const arena = buffer;
  struct DisplayResolution resolution;

  ifGetParam(display, IF_DISPLAY_RESOLUTION, &resolution);

  const uint16_t divX = resolution.width / 5;
  const uint16_t divY = resolution.height / 7;

  for (uint16_t y = 0; y < resolution.height; ++y)
  {
    switch (y / divY)
    {
      case 1:
      case 3:
        for (uint16_t x = 0; x < resolution.width; ++x)
        {
          const uint16_t column = x / divX;
          arena[x] = (column == 0 || column == 4) ? colorB : colorA;
        }
        break;

      case 2:
      case 4:
      case 5:
        for (uint16_t x = 0; x < resolution.width; ++x)
        {
          const uint16_t column = x / divX;
          arena[x] = (column == 1) ? colorA : colorB;
        }
        break;

      default:
        for (uint16_t x = 0; x < resolution.width; ++x)
          arena[x] = colorB;
        break;
    }

    ifWrite(display, arena, resolution.width * sizeof(uint16_t));
  }
}
/*----------------------------------------------------------------------------*/
void handleSolidFill(struct Interface *display, unsigned int color,
    unsigned int, void *buffer, size_t size)
{
  const unsigned int index = color % (COLORS_TOTAL + 1);
  const uint16_t value = index > 0 ?
      rgbTo565(makeColor(color)) : rgbTo565((Color){0, 0, 0});
  uint16_t * const arena = buffer;
  struct DisplayResolution resolution;

  ifGetParam(display, IF_DISPLAY_RESOLUTION, &resolution);

  const uint16_t lines = size / resolution.width;
  const struct DisplayWindow window = {
      .ax = 0,
      .ay = 0,
      .bx = resolution.width - 1,
      .by = resolution.height - 1
  };

  for (size_t i = 0; i < (size_t)lines * resolution.width; ++i)
    arena[i] = value;

  ifSetParam(display, IF_DISPLAY_WINDOW, &window);
  for (uint16_t row = 0; row < resolution.height;)
  {
    const size_t count = MIN(lines, resolution.height - row);

    ifWrite(display, arena, count * resolution.width * sizeof(uint16_t));
    row += count;
  }
}
