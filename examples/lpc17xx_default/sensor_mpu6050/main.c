/*
 * lpc17xx_default/sensor_mpu6050/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <dpm/sensors/mpu60xx.h>
#include <dpm/sensors/sensor_handler.h>
#include <halm/generic/work_queue.h>
#include <halm/pin.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/i2c.h>
#include <halm/platform/lpc/pin_int.h>
#include <halm/platform/lpc/serial.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define BUFFER_SIZE 512
#define INT_PIN     PIN(0, 2)
#define LED_PIN     PIN(1, 8)

enum
{
  SENSOR_TAG_ACCEL,
  SENSOR_TAG_GYRO,
  SENSOR_TAG_THERMO
};

struct Context
{
  struct Interface *serial;
  struct Sensor *sensors[3];
  struct Timer *chrono;
  struct Timer *timer;
  struct Pin led;

  bool enabled[3];
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
    .pin = INT_PIN,
    .event = INPUT_RISING,
    .pull = PIN_PULLDOWN
};

static const struct GpTimerConfig chronoTimerConfig = {
    .frequency = 1000000,
    .channel = 2
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

static const struct I2CConfig i2cConfig = {
    .rate = 400000,
    .scl = PIN(0, 11),
    .sda = PIN(0, 10),
    .channel = 2
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

  for (size_t i = 0; i < ARRAY_SIZE(context->sensors); ++i)
  {
    if (context->enabled[i])
      sensorSample(context->sensors[i]);
  }
}
/*----------------------------------------------------------------------------*/
static void onSensorData(void *argument, int tag __attribute__((unused)),
    const void *buffer, size_t length)
{
  struct Context * const context = argument;
  const unsigned long timestamp = timerGetValue(context->chrono);

  size_t count = 0;
  char text[64];

  if (tag == SENSOR_TAG_THERMO)
  {
    int32_t value;

    assert(length == sizeof(value));
    memcpy(&value, buffer, length);

    const char s = value >= 0 ? ' ' : '-';
    const int i = abs((int)value) / 256;
    const int q = ((value >= 0 ? value : -value) & 0xFF) * 1000 / 256;

    count = sprintf(text, "%lu T: %c%i.%03i C\r\n", timestamp, s, i, q);
  }
  else if (tag == SENSOR_TAG_ACCEL || tag == SENSOR_TAG_GYRO)
  {
    int32_t values[3];

    assert(length == sizeof(values));
    memcpy(&values, buffer, length);

    const char s[3] = {
        values[0] >= 0 ? ' ' : '-',
        values[1] >= 0 ? ' ' : '-',
        values[2] >= 0 ? ' ' : '-'
    };
    const int i[3] = {
        abs((int)values[0]) / 65536,
        abs((int)values[1]) / 65536,
        abs((int)values[2]) / 65536
    };
    const int q[3] = {
        ((values[0] >= 0 ? values[0] : -values[0]) & 0xFFFF) * 1000 / 65536,
        ((values[1] >= 0 ? values[1] : -values[1]) & 0xFFFF) * 1000 / 65536,
        ((values[2] >= 0 ? values[2] : -values[2]) & 0xFFFF) * 1000 / 65536
    };

    if (tag == SENSOR_TAG_ACCEL)
    {
      count = sprintf(text, "%lu a: %c%i.%03i %c%i.%03i %c%i.%03i g\r\n",
          timestamp,
          s[0], i[0], q[0],
          s[1], i[1], q[1],
          s[2], i[2], q[2]
      );
    }
    else
    {
      pinToggle(context->led);
      count = sprintf(text, "%lu w: %c%i.%03i %c%i.%03i %c%i.%03i rad/s\r\n",
          timestamp,
          s[0], i[0], q[0],
          s[1], i[1], q[1],
          s[2], i[2], q[2]
      );
    }
  }

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
      "\t1: toggle accelerometer\r\n"
      "\t2: toggle gyroscope\r\n"
      "\t3: toggle thermometer\r\n"
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
    for (size_t position = 0; position < count; ++position)
    {
      switch (buffer[position])
      {
        case '1':
        case '2':
        case '3':
        {
          const size_t i = buffer[position] - '1';

          context->enabled[i] = !context->enabled[i];
          break;
        }

        case 'a':
          if (context->automatic)
          {
            for (size_t i = 0; i < ARRAY_SIZE(context->sensors); ++i)
              sensorStop(context->sensors[i]);
          }
          else
          {
            for (size_t i = 0; i < ARRAY_SIZE(context->sensors); ++i)
            {
              if (context->enabled[i])
                sensorStart(context->sensors[i]);
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
          for (size_t i = 0; i < ARRAY_SIZE(context->sensors); ++i)
          {
            if (context->enabled[i])
              sensorReset(context->sensors[i]);
          }
          break;

        case 's':
        case ' ':
          for (size_t i = 0; i < ARRAY_SIZE(context->sensors); ++i)
          {
            if (context->enabled[i])
              sensorSample(context->sensors[i]);
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

  struct Interrupt * const event = init(PinInt, &eventConfig);
  assert(event != NULL);

  struct Timer * const chronoTimer = init(GpTimer, &chronoTimerConfig);
  assert(chronoTimer != NULL);
  timerEnable(chronoTimer);

  struct Timer * const sampleTimer = init(GpTimer, &sampleTimerConfig);
  assert(sampleTimer != NULL);
  timerSetOverflow(sampleTimer, 500000);

  struct Timer * const sensorTimer = init(GpTimer, &sensorTimerConfig);
  assert(sensorTimer != NULL);

  struct Interface * const serial = init(Serial, &serialConfig);
  assert(serial != NULL);

  struct Interface * const i2c = init(I2C, &i2cConfig);
  assert(i2c != NULL);

  const struct MPU60XXConfig mpuConfig = {
      .bus = i2c,
      .event = event,
      .timer = sensorTimer,
      .cs = 0,
      .address = 0x68,
      .rate = 400000,
      .sampleRate = 100,
      .accelScale = MPU60XX_ACCEL_16,
      .gyroScale = MPU60XX_GYRO_2000
  };
  struct MPU60XX * const mpu = init(MPU60XX, &mpuConfig);
  assert(mpu != NULL);

  /* Initialize Work Queue */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT != NULL);

  struct SensorHandler sh;
  shInit(&sh, 4, WQ_DEFAULT);

  struct MPU60XXProxy * const accel = mpu60xxMakeAccelerometer(mpu);
  assert(accel != NULL);

  shAttach(&sh, accel, SENSOR_TAG_ACCEL);
  sensorReset(accel);

  struct MPU60XXProxy * const gyro = mpu60xxMakeGyroscope(mpu);
  assert(gyro != NULL);

  shAttach(&sh, gyro, SENSOR_TAG_GYRO);
  sensorReset(gyro);

  struct MPU60XXProxy * const thermo = mpu60xxMakeThermometer(mpu);
  assert(thermo != NULL);

  shAttach(&sh, thermo, SENSOR_TAG_THERMO);
  sensorReset(thermo);

  struct Context context = {
      .serial = serial,
      .sensors = {
          (struct Sensor *)accel,
          (struct Sensor *)gyro,
          (struct Sensor *)thermo
      },
      .chrono = chronoTimer,
      .timer = sampleTimer,
      .led = led,
      .enabled = {
          true,
          true,
          true
      },
      .automatic = false,
      .manual = false,
      .queued = false
  };

  ifSetCallback(serial, onSerialEvent, &context);
  shSetDataCallback(&sh, onSensorData, &context);
  timerSetCallback(sampleTimer, onSampleRequest, &context);

  /* Start queue handler */
  wqStart(WQ_DEFAULT);

  return 0;
}
