/*
 * lpc17xx_default/sensor_xpt2046/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <dpm/sensors/sensor_handler.h>
#include <dpm/sensors/xpt2046.h>
#include <halm/generic/work_queue.h>
#include <halm/pin.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/pin_int.h>
#include <halm/platform/lpc/serial.h>
#include <halm/platform/lpc/spi_dma.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define CS_PIN      PIN(1, 15)
#define EV_PIN      PIN(2, 10)
#define LED_PIN     PIN(1, 8)

#define BUFFER_SIZE 512
#define SPI_CHANNEL 1

struct Context
{
  struct Interface *serial;
  struct Sensor *sensor;
  struct Timer *timer;
  struct Pin led;

  bool automatic;
  bool manual;
  bool queued;
};
/*----------------------------------------------------------------------------*/
static void onSampleRequest(void *);
static void onSensorData(void *, int, const void *, size_t);
static void onSerialEvent(void *);
static void serialHandlerTask(void *);
static void setupClock(void);
/*----------------------------------------------------------------------------*/
static const struct PinIntConfig eventConfig = {
    .pin = EV_PIN,
    .event = PIN_FALLING,
    .pull = PIN_PULLUP
};

static const struct GpTimerConfig sampleTimerConfig = {
    .frequency = 1000000,
    .channel = 0
};

static const struct GpTimerConfig sensorTimerConfig = {
    .frequency = 1000000,
    .channel = 1
};

static const struct SerialConfig serialConfig = {
    .rate = 500000,
    .rxLength = 16,
    .txLength = 1024,
    .rx = PIN(0, 16),
    .tx = PIN(0, 15),
    .channel = 1
};

static const struct SpiDmaConfig spiConfig[] = {
    {
        .rate = 2000000,
        .sck = PIN(0, 15),
        .miso = PIN(0, 17),
        .mosi = PIN(0, 18),
        .dma = {0, 1},
        .channel = 0,
        .mode = 0
    }, {
        .rate = 2000000,
        .sck = PIN(0, 7),
        .miso = PIN(0, 8),
        .mosi = PIN(0, 9),
        .dma = {3, 2},
        .channel = 1,
        .mode = 0
    }
};

static const struct WorkQueueConfig workQueueConfig = {
    .size = 4
};
/*----------------------------------------------------------------------------*/
static struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};

static const struct GenericClockConfig mainClockConfig = {
    .source = CLOCK_PLL
};

static const struct PllConfig sysPllConfig = {
    .source = CLOCK_EXTERNAL,
    .divisor = 4,
    .multiplier = 32
};
/*----------------------------------------------------------------------------*/
static void onSampleRequest(void *argument)
{
  struct Context * const context = argument;
  sensorSample(context->sensor);
}

/*----------------------------------------------------------------------------*/
static void onSensorData(void *argument, int tag __attribute__((unused)),
    const void *buffer, size_t length)
{
  struct Context * const context = argument;
  int16_t values[3];

  assert(length == sizeof(values));
  memcpy(values, buffer, length);

  size_t count;
  char text[64];

  pinToggle(context->led);
  count = sprintf(text, "%i %i %i\r\n", values[0], values[1], values[2]);

  ifWrite(context->serial, text, count);
}
/*----------------------------------------------------------------------------*/
static void onSerialEvent(void *argument)
{
  struct Context * const context = argument;

  if (!context->queued)
  {
    if (wqAdd(WQ_DEFAULT, serialHandlerTask, argument) == E_OK)
      context->queued = true;
  }
}
/*----------------------------------------------------------------------------*/
static void serialHandlerTask(void *argument)
{
static const char HELP_MESSAGE[] =
      "Shortcuts:\r\n"
      "\ta: automatic mode\r\n"
      "\th: show this help message\r\n"
      "\tm: time-triggered mode\r\n"
      "\ts: read sample\r\n";

  struct Context * const context = argument;
  char buffer[BUFFER_SIZE];
  size_t count;

  context->queued = false;

  while ((count = ifRead(context->serial, buffer, sizeof(buffer))) > 0)
  {
    for (size_t i = 0; i < count; ++i)
    {
      switch (buffer[i])
      {
        case 'a':
          if (context->automatic)
            sensorStop(context->sensor);
          else
            sensorStart(context->sensor);
          context->automatic = !context->automatic;
          break;

        case 'h':
          ifWrite(context->serial, HELP_MESSAGE, sizeof(HELP_MESSAGE));
          break;

        case 'm':
          if (context->manual)
            timerDisable(context->timer);
          else
            timerEnable(context->timer);
          context->manual = !context->manual;
          break;

        case 's':
        case ' ':
          sensorSample(context->sensor);
          context->automatic = false;
          break;
      }
    }
  }
}
/*----------------------------------------------------------------------------*/
static void setupClock(void)
{
  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(SystemPll, &sysPllConfig);
  while (!clockReady(SystemPll));

  clockEnable(MainClock, &mainClockConfig);
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  setupClock();

  const struct Pin led = pinInit(LED_PIN);
  pinOutput(led, false);

  struct Interrupt * const event = init(PinInt, &eventConfig);
  assert(event);

  struct Timer * const sampleTimer = init(GpTimer, &sampleTimerConfig);
  assert(sampleTimer);
  timerSetOverflow(sampleTimer, 500000);

  struct Timer * const sensorTimer = init(GpTimer, &sensorTimerConfig);
  assert(sensorTimer);

  struct Interface * const serial = init(Serial, &serialConfig);
  assert(serial);

  struct Interface * const spi = init(SpiDma, &spiConfig[SPI_CHANNEL]);
  assert(spi);

  const struct XPT2046Config touchConfig = {
      .bus = spi,
      .event = event,
      .timer = sensorTimer,
      .cs = CS_PIN,
      .rate = 100000,
      .threshold = 100,
      .x = 240,
      .y = 320
  };
  struct XPT2046 * const touch = init(XPT2046, &touchConfig);
  assert(touch);
  xpt2046ResetCalibration(touch);

  /* Initialize Work Queue */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT);

  struct SensorHandler sh;
  shInit(&sh, 4, WQ_DEFAULT);

  shAttach(&sh, touch, 0);
  sensorReset(touch);

 struct Context context = {
      .serial = serial,
      .sensor = (struct Sensor *)touch,
      .timer = sampleTimer,
      .led = led,
      .automatic = false,
      .manual = false,
      .queued = false
  };

  ifSetCallback(serial, onSerialEvent, &context);
  shSetCallback(&sh, onSensorData, &context);
  timerSetCallback(sampleTimer, onSampleRequest, &context);

  /* Start queue handler */
  wqStart(WQ_DEFAULT);

  return 0;
}
