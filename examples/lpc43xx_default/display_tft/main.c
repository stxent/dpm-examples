/*
 * lpc43xx_default/display_tft/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <dpm/displays/display.h>
#include <dpm/displays/ili9325.h>
#include <dpm/displays/s6d1121.h>
#include <dpm/platform/lpc/sgpio_bus.h>
#include <halm/pin.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/serial.h>
#include <xcore/memory.h>
#include <assert.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
#define DISPLAY_ILI9325
/* #define DISPLAY_S6D1121 */
/*----------------------------------------------------------------------------*/
#define COLORS_TOTAL      7
#define DISPLAY_BL_PIN    PIN(PORT_8, 5)
#define DISPLAY_CS_PIN    PIN(PORT_8, 1)
#define DISPLAY_RESET_PIN PIN(PORT_C, 6)
#define DISPLAY_RS_PIN    PIN(PORT_C, 7)
#define DISPLAY_RW_PIN    PIN(PORT_C, 3)

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
static const struct SgpioBusConfig sgpioBusConfig = {
    .prescaler = 4,
    .dma = 0,
    .priority = 0,
    .inversion = false,

    .pins = {
        .clock = PIN(PORT_2, 0), /* SGPIO4 */
        .dma = PIN(PORT_1, 16),  /* SGPIO3 */
        .data = {
            PIN(PORT_4, 2),
            PIN(PORT_4, 3),
            PIN(PORT_1, 14),
            PIN(PORT_4, 5),
            PIN(PORT_4, 6),
            PIN(PORT_4, 8),
            PIN(PORT_4, 9),
            PIN(PORT_4, 10)
        }
    },

    .slices = {
        .gate = SGPIO_SLICE_P,
        .qualifier = SGPIO_SLICE_A /* SGPIO0 */
    }
};
/*----------------------------------------------------------------------------*/
static const struct GenericClockConfig initialClockConfig = {
    .source = CLOCK_INTERNAL
};

static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000,
    .bypass = false
};

static const struct GenericClockConfig mainClockConfig = {
    .source = CLOCK_PLL
};

static const struct PllConfig sysPllConfig = {
    .source = CLOCK_EXTERNAL,
    .divisor = 1,
    .multiplier = 17
};

static const struct SerialConfig serialConfig = {
    .rxLength = 32,
    .txLength = 32,
    .rate = 19200,
    .rx = PIN(PORT_F, 11),
    .tx = PIN(PORT_F, 10),
    .channel = 0
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
static void setupClock(void)
{
  clockEnable(MainClock, &initialClockConfig);

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(SystemPll, &sysPllConfig);
  while (!clockReady(SystemPll));

  clockEnable(MainClock, &mainClockConfig);

  clockEnable(PeriphClock, &mainClockConfig);
  while (!clockReady(PeriphClock));

  clockEnable(Usart3Clock, &mainClockConfig);
  while (!clockReady(Usart3Clock));
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  bool event = false;

  setupClock();

  const struct Pin pinBL = pinInit(DISPLAY_BL_PIN);
  pinOutput(pinBL, true);

  const struct Pin pinRW = pinInit(DISPLAY_RW_PIN);
  pinOutput(pinRW, false);

  struct Interface * const serial = init(Serial, &serialConfig);
  assert(serial);
  ifSetCallback(serial, onSerialEvent, &event);

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer);
  timerEnable(timer);

  struct Interface * const memoryBus = init(SgpioBus, &sgpioBusConfig);
  assert(memoryBus);

#if defined(DISPLAY_ILI9325)
  const struct ILI9325Config displayConfig = {
      .bus = memoryBus,
      .cs = DISPLAY_CS_PIN,
      .reset = DISPLAY_RESET_PIN,
      .rs = DISPLAY_RS_PIN
  };

  struct Interface * const display = init(ILI9325, &displayConfig);
  assert(display);
#elif defined(DISPLAY_S6D1121)
  const struct S6D1121Config displayConfig = {
      .bus = memoryBus,
      .cs = DISPLAY_CS_PIN,
      .reset = DISPLAY_RESET_PIN,
      .rs = DISPLAY_RS_PIN
  };

  struct Interface * const display = init(S6D1121, &displayConfig);
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
      .color = 40,
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
