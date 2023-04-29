/*
 * lpc17xx_default/sensor_ds18b20/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <dpm/sensors/ds18b20.h>
#include <dpm/sensors/sensor_handler.h>
#include <halm/generic/software_timer.h>
#include <halm/generic/work_queue.h>
#include <halm/pin.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/one_wire_ssp.h>
#include <halm/platform/lpc/serial.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define BUFFER_SIZE 64
#define MAX_SENSORS 8

#define LED_PIN     PIN(1, 8)
#define PWR_PIN     PIN(0, 3)

struct Context
{
  struct Interface *serial;
  struct Sensor *sensors[MAX_SENSORS];
  struct Timer *timer;
  struct Pin led;

  bool automatic;
  bool manual;
  bool queued;
};
/*----------------------------------------------------------------------------*/
static void onSampleRequest(void *);
static void onSearchCompleted(void *);
static void onSensorData(void *, int, const void *, size_t);
static void onSerialEvent(void *);
static void serialHandlerTask(void *);
static void setupClock(void);
/*----------------------------------------------------------------------------*/
static const struct OneWireSspConfig owConfig = {
    .miso = PIN(0, 8),
    .mosi = PIN(0, 9),
    .channel = 1
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

  for (size_t i = 0; i < MAX_SENSORS; ++i)
  {
    if (context->sensors[i])
      sensorSample(context->sensors[i]);
  }
}
/*----------------------------------------------------------------------------*/
static void onSearchCompleted(void *argument)
{
  *(bool *)argument = true;
}
/*----------------------------------------------------------------------------*/
static void onSensorData(void *argument, int tag, const void *buffer,
    size_t length)
{
  struct Context * const context = argument;
  int16_t value;

  assert(length == sizeof(value));
  memcpy(&value, buffer, length);

  const char s = value >= 0 ? ' ' : '-';
  const int i = abs((int)value) / 16;
  const int q = ((value >= 0 ? value : -value) & 0xF) * 1000 / 16;

  size_t count;
  char text[64];

  pinToggle(context->led);
  count = sprintf(text, "%i: %c%i.%03i\r\n", tag, s, i, q);

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
      "\tr: reset sensor\r\n"
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
          for (size_t sensor = 0; sensor < MAX_SENSORS; ++sensor)
          {
            if (context->sensors[sensor])
            {
              if (context->automatic)
                sensorStop(context->sensors[sensor]);
              else
                sensorStart(context->sensors[sensor]);
            }
          }
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

        case 'r':
          for (size_t sensor = 0; sensor < MAX_SENSORS; ++sensor)
          {
            if (context->sensors[sensor])
              sensorReset(context->sensors[sensor]);
          }
          break;

        case 's':
        case ' ':
          for (size_t sensor = 0; sensor < MAX_SENSORS; ++sensor)
          {
            if (context->sensors[sensor])
              sensorSample(context->sensors[sensor]);
          }
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
  const struct Pin pwr = pinInit(PWR_PIN);
  pinOutput(pwr, false);

  struct Interface * const ow = init(OneWireSsp, &owConfig);
  assert(ow != NULL);

  struct Interface * const serial = init(Serial, &serialConfig);
  assert(serial != NULL);

  struct Timer * const sampleTimer = init(GpTimer, &sampleTimerConfig);
  assert(sampleTimer != NULL);
  timerSetOverflow(sampleTimer, 1000000);

  /* Initialize software timer factory */

  struct Timer * const sensorTimer = init(GpTimer, &sensorTimerConfig);
  assert(sensorTimer != NULL);
  timerSetOverflow(sensorTimer, 1000);

  const struct SoftwareTimerFactoryConfig factoryConfig = {
      .timer = sensorTimer
  };
  struct SoftwareTimerFactory * const factory =
      init(SoftwareTimerFactory, &factoryConfig);
  assert(factory != NULL);

  /* Initialize Work Queue */

  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT != NULL);

  struct SensorHandler sh;
  shInit(&sh, MAX_SENSORS, WQ_DEFAULT);

  /* Find all sensors on the bus and store them in the context structure */

  struct Context context = {
      .serial = serial,
      .sensors = {0},
      .timer = sampleTimer,
      .led = led,
      .automatic = false,
      .manual = false,
      .queued = false
  };
  size_t tag = 0;
  bool event = false;

  ifSetCallback(ow, onSearchCompleted, &event);
  ifSetParam(ow, IF_ONE_WIRE_START_SEARCH, NULL);

  do
  {
    while (!event)
      barrier();
    event = false;

    if (ifGetParam(ow, IF_STATUS, NULL) != E_OK)
      break;

    uint64_t address;

    if (ifGetParam(ow, IF_ADDRESS_64, &address) == E_OK)
    {
      const struct DS18B20Config thermoConfig = {
          .bus = ow,
          .timer = softwareTimerCreate(factory),
          .address = address,
          .resolution = DS18B20_RESOLUTION_DEFAULT
      };
      assert(thermoConfig.timer != NULL);

      struct DS18B20 * const thermo = init(DS18B20, &thermoConfig);
      assert(thermo != NULL);

      context.sensors[tag] = (struct Sensor *)thermo;
      shAttach(&sh, thermo, (int)tag);
      sensorReset(thermo);

      char text[64];
      const size_t count = sprintf(text,
          "%02X.%02X.%02X.%02X.%02X.%02X.%02X.%02X: registered as %i\r\n",
          (unsigned int)((address >> 56) & 0xFF),
          (unsigned int)((address >> 48) & 0xFF),
          (unsigned int)((address >> 40) & 0xFF),
          (unsigned int)((address >> 32) & 0xFF),
          (unsigned int)((address >> 24) & 0xFF),
          (unsigned int)((address >> 16) & 0xFF),
          (unsigned int)((address >> 8) & 0xFF),
          (unsigned int)(address & 0xFF),
          (int)tag
      );

      ifWrite(serial, text, count);
      ++tag;
    }
  }
  while (ifSetParam(ow, IF_ONE_WIRE_FIND_NEXT, NULL) == E_OK);

  ifSetCallback(serial, onSerialEvent, &context);
  shSetDataCallback(&sh, onSensorData, &context);
  timerSetCallback(sampleTimer, onSampleRequest, &context);

  /* Start queue handler and software timers */
  timerEnable(sensorTimer);
  wqStart(WQ_DEFAULT);

  return 0;
}
