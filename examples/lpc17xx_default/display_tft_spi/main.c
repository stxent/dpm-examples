/*
 * lpc17xx_default/display_tft_spi/main.c
 * Copyright (C) 2021 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <dpm/displays/display.h>
#include <dpm/displays/st7735.h>
#include <halm/pin.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/serial.h>
#include <halm/platform/lpc/spi.h>
#include <halm/platform/lpc/spi_dma.h>
#include <xcore/memory.h>
#include <assert.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
#define SPI_CHANNEL 1
#define SPI_DMA

#define DISPLAY_ST7735
/*----------------------------------------------------------------------------*/
#define COLORS_TOTAL      7
#define DISPLAY_BL_PIN    PIN(1, 26)
#define DISPLAY_CS_PIN    PIN(0, 6)
#define DISPLAY_RESET_PIN PIN(4, 29)
#define DISPLAY_RS_PIN    PIN(4, 28)

enum
{
  PAGE_SOLID,
  PAGE_GRADIENT,
  PAGE_LINES,
  PAGE_CHESS,
  PAGE_MARKER,
  PAGE_END
};

struct Color
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct Context
{
  struct Interface *display;
  struct Interface *serial;
  struct Timer *timer;

  struct DisplayResolution resolution;
  struct DisplayWindow window;
  enum DisplayOrientation orientation;
  unsigned int color;
  unsigned int index;
  unsigned int page;
};
/*----------------------------------------------------------------------------*/
static struct Color makeColor(unsigned int);
static uint16_t rgbTo565(struct Color);
/*----------------------------------------------------------------------------*/
static uint16_t arena[12288];
/*----------------------------------------------------------------------------*/
#ifdef SPI_DMA
#define SPI_CLASS SpiDma
#else
#define SPI_CLASS Spi
#endif

#ifdef SPI_DMA
static const struct SpiDmaConfig spiConfig[] = {
    {
        .rate = 25000000,
        .sck = PIN(0, 15),
        .miso = PIN(0, 17),
        .mosi = PIN(0, 18),
        .dma = {0, 1},
        .channel = 0,
        .mode = 0
    }, {
        .rate = 25000000,
        .sck = PIN(0, 7),
        .miso = PIN(0, 8),
        .mosi = PIN(0, 9),
        .dma = {3, 2},
        .channel = 1,
        .mode = 0
    }
};
#else
static const struct SpiConfig spiConfig[] = {
    {
        .rate = 25000000,
        .sck = PIN(0, 15),
        .miso = PIN(0, 17),
        .mosi = PIN(0, 18),
        .channel = 0,
        .mode = 0
    }, {
        .rate = 25000000,
        .sck = PIN(0, 7),
        .miso = PIN(0, 8),
        .mosi = PIN(0, 9),
        .channel = 1,
        .mode = 0
    }
};
#endif
/*----------------------------------------------------------------------------*/
static struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};

static struct GenericClockConfig mainClkConfig = {
    .source = CLOCK_PLL
};

static struct PllConfig sysPllConfig = {
    .multiplier = 25,
    .divisor = 3,
    .source = CLOCK_EXTERNAL
};

static const struct SerialConfig serialConfig = {
    .rate = 19200,
    .rxLength = 32,
    .txLength = 32,
    .rx = PIN(0, 16),
    .tx = PIN(0, 15),
    .channel = 1
};

static const struct GpTimerConfig timerConfig = {
    .channel = 3,
    .frequency = 1000000,
    .event = 0
};
/*----------------------------------------------------------------------------*/
static struct Color interpolateColor(struct Color a, struct Color b,
    int current, int total)
{
  const struct Color result = {
      .r = a.r + (b.r - a.r) * current / total,
      .g = a.g + (b.g - a.g) * current / total,
      .b = a.b + (b.b - a.b) * current / total
  };
  return result;
}
/*----------------------------------------------------------------------------*/
static struct Color makeColor(unsigned int color)
{
  static const struct Color COLOR_TABLE[] = {
      {255, 255, 255}, /* White */
      {255, 0, 0},     /* Red */
      {255, 255, 0},   /* Yellow */
      {0, 255, 0},     /* Green */
      {0, 255, 255},   /* Cyan */
      {0, 0, 255},     /* Blue */
      {255, 0, 255}    /* Magenta */
  };
  const size_t index = (size_t)color % ARRAY_SIZE(COLOR_TABLE);

  return COLOR_TABLE[index];
}
/*----------------------------------------------------------------------------*/
static uint16_t rgbTo565(struct Color color)
{
  const uint8_t r = color.r * 32 / 256;
  const uint8_t g = color.g * 64 / 256;
  const uint8_t b = color.b * 32 / 256;

  return toBigEndian16((r << 11) | (g << 5) | b);
}
/*----------------------------------------------------------------------------*/
static void handleChessFill(struct Context *context)
{
  const uint16_t colorA = context->index & 1 ?
      rgbTo565(makeColor(context->color)) : rgbTo565((struct Color){0, 0, 0});
  const uint16_t colorB = context->index & 1 ?
      rgbTo565((struct Color){0, 0, 0}) : rgbTo565(makeColor(context->color));
  const uint16_t height = context->resolution.height;
  const uint16_t width = context->resolution.width;
  const uint16_t div = width / 8;

  for (uint16_t y = 0; y < height; ++y)
  {
    if ((y / div) & 1)
    {
      for (uint16_t x = 0; x < width; ++x)
        arena[x] = ((x / div) & 1) ? colorA : colorB;
    }
    else
    {
      for (uint16_t x = 0; x < width; ++x)
        arena[x] = ((x / div) & 1) ? colorB : colorA;
    }

    ifWrite(context->display, arena, width * sizeof(uint16_t));
  }
}
/*----------------------------------------------------------------------------*/
static void handleGradientFill(struct Context *context)
{
  const struct Color colorA = context->index & 1 ?
      makeColor(context->color) : (struct Color){0, 0, 0};
  const struct Color colorB = context->index & 1 ?
      (struct Color){0, 0, 0} : makeColor(context->color);
  const uint16_t height = context->resolution.height;
  const uint16_t width = context->resolution.width;

  for (uint16_t y = 0; y < height; ++y)
  {
    const uint16_t color =
        rgbTo565(interpolateColor(colorA, colorB, y, height));

    for (uint16_t x = 0; x < width; ++x)
      arena[x] = color;

    ifWrite(context->display, arena, width * sizeof(uint16_t));
  }
}
/*----------------------------------------------------------------------------*/
static void handleLineFill(struct Context *context)
{
  const unsigned int index = context->index % 3;
  const uint16_t colorA = rgbTo565(makeColor(context->color));
  const uint16_t colorB = rgbTo565((struct Color){0, 0, 0});
  const uint16_t height = context->resolution.height;
  const uint16_t width = context->resolution.width;
  const uint16_t lines = (ARRAY_SIZE(arena) / width) & ~1;

  switch (index)
  {
    case 1:
      for (size_t i = 0; i < (size_t)lines * width; ++i)
        arena[i] = (i & 1) ? colorA : colorB;
      break;

    case 2:
      for (uint16_t y = 0; y < lines; ++y)
      {
        for (uint16_t x = 0; x < width; ++x)
        {
          if (y & 1)
            arena[y * width + x] = colorA;
          else
            arena[y * width + x] = colorB;
        }
      }
      break;

    default:
      for (uint16_t y = 0; y < lines; ++y)
      {
        for (uint16_t x = 0; x < width; ++x)
        {
          if (y & 1)
            arena[y * width + x] = (x & 1) ? colorA : colorB;
          else
            arena[y * width + x] = (x & 1) ? colorB : colorA;
        }
      }
      break;
  }

  for (uint16_t row = 0; row < height;)
  {
    const size_t count = MIN(lines, height - row);

    ifWrite(context->display, arena, count * width * sizeof(uint16_t));
    row += count;
  }
}
/*----------------------------------------------------------------------------*/
static void handleMarkerFill(struct Context *context)
{
  const uint16_t colorA = context->index & 1 ?
      rgbTo565((struct Color){0, 0, 0}) : rgbTo565(makeColor(context->color));
  const uint16_t colorB = context->index & 1 ?
      rgbTo565(makeColor(context->color)) : rgbTo565((struct Color){0, 0, 0});
  const uint16_t height = context->resolution.height;
  const uint16_t width = context->resolution.width;
  const uint16_t divX = width / 5;
  const uint16_t divY = height / 7;

  for (uint16_t y = 0; y < height; ++y)
  {
    switch (y / divY)
    {
      case 1:
      case 3:
        for (uint16_t x = 0; x < width; ++x)
        {
          const uint16_t column = x / divX;
          arena[x] = (column == 0 || column == 4) ? colorB : colorA;
        }
        break;

      case 2:
      case 4:
      case 5:
        for (uint16_t x = 0; x < width; ++x)
        {
          const uint16_t column = x / divX;
          arena[x] = (column == 1) ? colorA : colorB;
        }
        break;

      default:
        for (uint16_t x = 0; x < width; ++x)
          arena[x] = colorB;
        break;
    }

    ifWrite(context->display, arena, width * sizeof(uint16_t));
  }
}
/*----------------------------------------------------------------------------*/
static void handleSolidFill(struct Context *context)
{
  const unsigned int index = context->color % (COLORS_TOTAL + 1);
  const uint16_t color = index > 0 ?
      rgbTo565(makeColor(context->color)) : rgbTo565((struct Color){0, 0, 0});
  const uint16_t lines = ARRAY_SIZE(arena) / context->resolution.width;

  for (size_t i = 0; i < (size_t)lines * context->resolution.width; ++i)
    arena[i] = color;

  for (uint16_t row = 0; row < context->resolution.height;)
  {
    const size_t count = MIN(lines, context->resolution.height - row);

    ifWrite(context->display, arena,
        count * context->resolution.width * sizeof(uint16_t));
    row += count;
  }
}
/*----------------------------------------------------------------------------*/
static void handlePageChange(struct Context *context)
{
  const uint32_t start = timerGetValue(context->timer);

  switch (context->page)
  {
    case 0:
      handleSolidFill(context);
      break;

    case 1:
      handleGradientFill(context);
      break;

    case 2:
      handleLineFill(context);
      break;

    case 3:
      handleChessFill(context);
      break;

    case 4:
      handleMarkerFill(context);
      break;

    default:
      break;
  }

  const uint32_t passed = timerGetValue(context->timer) - start;
  char text[24];
  const size_t length = sprintf(text, "%lu us\r\n", (unsigned long)passed);

  ifWrite(context->serial, text, length);
}
/*----------------------------------------------------------------------------*/
static void handleColorChange(struct Context *context)
{
  ++context->color;
  handlePageChange(context);
}
/*----------------------------------------------------------------------------*/
static void handleOrientationChange(struct Context *context)
{
  if (++context->orientation == DISPLAY_ORIENTATION_END)
    context->orientation = DISPLAY_ORIENTATION_NORMAL;

  const uint32_t orientation = (uint32_t)context->orientation;
  ifSetParam(context->display, IF_DISPLAY_ORIENTATION, &orientation);

  handlePageChange(context);
}
/*----------------------------------------------------------------------------*/
static void onSerialEvent(void *argument)
{
  *(bool *)argument = true;
}
/*----------------------------------------------------------------------------*/
static void parseInput(struct Context *context, char input)
{
  if (input >= '1' && input <= '5')
  {
    const unsigned int page = (int)(input - '1');

    if (context->page != page)
      context->index = 0;
    else
      ++context->index;
    context->page = page;

    handlePageChange(context);
  }
  else if (input == 'c')
  {
    handleColorChange(context);
  }
  else if (input == 'r')
  {
    handleOrientationChange(context);
  }
}
/*----------------------------------------------------------------------------*/
static void setupClock()
{
  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(SystemPll, &sysPllConfig);
  while (!clockReady(SystemPll));

  clockEnable(MainClock, &mainClkConfig);
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  bool event = false;

  setupClock();

  const struct Pin pinBL = pinInit(DISPLAY_BL_PIN);
  pinOutput(pinBL, true);

  struct Interface * const serial = init(Serial, &serialConfig);
  assert(serial);
  ifSetCallback(serial, onSerialEvent, &event);

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer);
  timerEnable(timer);

  struct Interface * const spi = init(SPI_CLASS, &spiConfig[SPI_CHANNEL]);
  assert(spi);

#if defined(DISPLAY_ST7735)
  const struct ST7735Config displayConfig = {
      .bus = spi,
      .cs = DISPLAY_CS_PIN,
      .reset = DISPLAY_RESET_PIN,
      .rs = DISPLAY_RS_PIN
  };

  struct Interface * const display = init(ST7735, &displayConfig);
  assert(display);
#else
#  error "Display type is not specified"
#endif

  const uint32_t orientation = DISPLAY_ORIENTATION_NORMAL;
  ifSetParam(display, IF_DISPLAY_ORIENTATION, &orientation);

  struct DisplayResolution resolution;
  ifGetParam(display, IF_DISPLAY_RESOLUTION, &resolution);

  struct Context context = {
      .display = display,
      .serial = serial,
      .timer = timer,
      .resolution = {
          .width = resolution.width,
          .height = resolution.height
      },
      .window = {
          .ax = 0,
          .ay = 0,
          .bx = resolution.width - 1,
          .by = resolution.height - 1
      },
      .orientation = DISPLAY_ORIENTATION_NORMAL,
      .color = 0,
      .index = 0,
      .page = 0
  };

  handleSolidFill(&context);

  while (1)
  {
    if (!event)
      barrier();
    event = false;

    char input[16];
    const size_t count = ifRead(serial, input, sizeof(input));

    for (size_t i = 0; i < count; ++i)
      parseInput(&context, input[i]);
  }

  return 0;
}
