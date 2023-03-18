/*
 * lpc43xx_default/spifi_w25/main.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <dpm/memory/w25_spim.h>
#include <halm/generic/flash.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/spifi.h>
#include <assert.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define BOARD_LED_0 PIN(PORT_5, 7)
#define BOARD_LED_1 PIN(PORT_5, 5)
/*----------------------------------------------------------------------------*/
static bool memoryTestSequence(struct Interface *, struct Interface *);
static bool memoryTestSequenceZerocopy(struct Interface *);
static void onMemoryEvent(void *);
static void onTimerOverflow(void *);
static void setupClock(void);
/*----------------------------------------------------------------------------*/
static const struct SpifiConfig spifiConfig = {
    .cs = PIN(PORT_3, 8),
    .io0 = PIN(PORT_3, 7),
    .io1 = PIN(PORT_3, 6),
    .io2 = PIN(PORT_3, 5),
    .io3 = PIN(PORT_3, 4),
    .sck = PIN(PORT_3, 3),
    .channel = 0,
    .mode = 0,
    .dma = 0
};

static const struct GpTimerConfig timerConfig = {
    .channel = 0,
    .frequency = 1000000,
    .event = 0
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
    .source = CLOCK_EXTERNAL
};
/*----------------------------------------------------------------------------*/
static bool memoryTestSequence(struct Interface *spifi,
    struct Interface *memory)
{
  static uint32_t address = 0;
  const uint32_t position = address;

  uint8_t buffer[1024];
  uint32_t capacity = 0;
  uint32_t chunk;
  uint32_t count;
  enum Result res;

  /* Read geometry */

  res = ifGetParam(memory, IF_SIZE, &capacity);
  if (res != E_OK || capacity == 0)
    return false;

  chunk = 0;
  res = ifGetParam(memory, IF_FLASH_SECTOR_SIZE, &chunk);
  if (res != E_OK || chunk == 0)
    return false;

  chunk = 0;
  res = ifGetParam(memory, IF_FLASH_BLOCK_SIZE, &chunk);
  if (res != E_OK || chunk == 0)
    return false;

  address += chunk;
  if (address >= capacity)
    address = 0;

  /* Erase */

  res = ifSetParam(memory, IF_FLASH_ERASE_BLOCK, &position);
  if (res != E_OK)
    return false;

  /* Verify erase */

  res = ifSetParam(memory, IF_POSITION, &position);
  if (res != E_OK)
    return false;
  count = ifRead(memory, buffer, sizeof(buffer));
  if (count != sizeof(buffer))
    return false;

  for (size_t i = 0; i < sizeof(buffer); ++i)
  {
    if (buffer[i] != 0xFF)
      return false;
  }

  /* Program */

  for (size_t i = 0; i < sizeof(buffer); ++i)
    buffer[i] = (uint8_t)i;

  res = ifSetParam(memory, IF_POSITION, &position);
  if (res != E_OK)
    return false;
  count = ifWrite(memory, buffer, sizeof(buffer));
  if (count != sizeof(buffer))
    return false;

  /* Verify program */

  res = ifSetParam(memory, IF_POSITION, &position);
  if (res != E_OK)
    return false;
  count = ifRead(memory, buffer, sizeof(buffer));
  if (count != sizeof(buffer))
    return false;

  for (size_t i = 0; i < sizeof(buffer); ++i)
  {
    if (buffer[i] != (uint8_t)i)
      return false;
  }

  /* Verify memory mapping mode */

  const uint8_t * const region = spifiAddress((struct Spifi *)spifi);

  w25MemoryMappingEnable((struct W25SPIM *)memory);
  for (size_t i = 0; i < sizeof(buffer); ++i)
  {
    if (region[position + i] != (uint8_t)i)
    {
      w25MemoryMappingDisable((struct W25SPIM *)memory);
      return false;
    }
  }
  w25MemoryMappingDisable((struct W25SPIM *)memory);

  return true;
}
/*----------------------------------------------------------------------------*/
static bool memoryTestSequenceZerocopy(struct Interface *memory)
{
  static uint32_t address = 1024 * 1024;
  const uint32_t position = address;

  uint8_t buffer[1024];
  uint32_t capacity = 0;
  uint32_t chunk = 0;
  uint32_t count;
  enum Result res;
  bool event = false;

  ifSetParam(memory, IF_ZEROCOPY, 0);
  ifSetCallback(memory, onMemoryEvent, &event);

  /* Read geometry */

  res = ifGetParam(memory, IF_SIZE, &capacity);
  if (res == E_OK && capacity == 0)
    res = E_MEMORY;

  if (res == E_OK)
  {
    res = ifGetParam(memory, IF_FLASH_BLOCK_SIZE, &chunk);
    if (res == E_OK && chunk == 0)
      res = E_MEMORY;

    address += chunk;
    if (address >= capacity)
      address = 0;
  }

  /* Erase */

  if (res == E_OK)
  {
    res = ifSetParam(memory, IF_FLASH_ERASE_BLOCK, &position);
    if (res == E_OK)
    {
      while (!event)
        barrier();

      event = false;
      res = ifGetParam(memory, IF_STATUS, 0);
    }
  }

  /* Verify erase */

  if (res == E_OK)
    res = ifSetParam(memory, IF_POSITION, &position);
  if (res == E_OK)
  {
    count = ifRead(memory, buffer, sizeof(buffer));
    if (count == sizeof(buffer))
    {
      while (!event)
        barrier();

      event = false;
      res = ifGetParam(memory, IF_STATUS, 0);

      if (res == E_OK)
      {
        for (size_t i = 0; i < sizeof(buffer); ++i)
        {
          if (buffer[i] != 0xFF)
          {
            res = E_VALUE;
            break;
          }
        }
      }
    }
    else
      res = E_INTERFACE;
  }

  /* Program */

  if (res == E_OK)
    res = ifSetParam(memory, IF_POSITION, &position);
  if (res == E_OK)
  {
    for (size_t i = 0; i < sizeof(buffer); ++i)
      buffer[i] = (uint8_t)i;

    count = ifWrite(memory, buffer, sizeof(buffer));
    if (count == sizeof(buffer))
    {
      while (!event)
        barrier();

      event = false;
      res = ifGetParam(memory, IF_STATUS, 0);
    }
    else
      res = E_INTERFACE;
  }

  /* Verify program */

  if (res == E_OK)
    res = ifSetParam(memory, IF_POSITION, &position);
  if (res == E_OK)
  {
    count = ifRead(memory, buffer, sizeof(buffer));
    if (count == sizeof(buffer))
    {
      while (!event)
        barrier();

      event = false;
      res = ifGetParam(memory, IF_STATUS, 0);

      if (res == E_OK)
      {
        for (size_t i = 0; i < sizeof(buffer); ++i)
        {
          if (buffer[i] != (uint8_t)i)
          {
            res = E_VALUE;
            break;
          }
        }
      }
    }
    else
      res = E_INTERFACE;
  }

  ifSetCallback(memory, 0, 0);
  ifSetParam(memory, IF_BLOCKING, 0);
  return res == E_OK;
}
/*----------------------------------------------------------------------------*/
static void onMemoryEvent(void *argument)
{
  *(bool *)argument = true;
}
/*----------------------------------------------------------------------------*/
static void onTimerOverflow(void *argument)
{
  *(bool *)argument = true;
}
/*----------------------------------------------------------------------------*/
static void setupClock(void)
{
  clockEnable(MainClock, &initialClockConfig);

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(MainClock, &mainClockConfig);

  clockEnable(SpifiClock, &mainClockConfig);
  while (!clockReady(SpifiClock));
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  bool event = false;

  setupClock();

  const struct Pin blockingLed = pinInit(BOARD_LED_0);
  pinOutput(blockingLed, false);
  const struct Pin zerocopyLed = pinInit(BOARD_LED_1);
  pinOutput(zerocopyLed, false);

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer);

  struct Interface * const spifi = init(Spifi, &spifiConfig);
  assert(spifi);

  const struct W25SPIMConfig w25Config = {
      .spim = spifi,
      .shrink = false,
      .dtr = true,
      .qpi = true
  };
  struct Interface * const memory = init(W25SPIM, &w25Config);
  assert(memory);

  timerSetOverflow(timer, timerGetFrequency(timer) * 5);
  timerSetCallback(timer, onTimerOverflow, &event);
  timerEnable(timer);

  while (1)
  {
    while (!event)
      barrier();
    event = false;

    pinSet(blockingLed);
    if (memoryTestSequence(spifi, memory))
      pinReset(blockingLed);

    pinSet(zerocopyLed);
    if (memoryTestSequenceZerocopy(memory))
      pinReset(zerocopyLed);
  }

  return 0;
}
