/*
 * lpc43xx_default/spifi_w25/main.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <dpm/memory/w25_spim.h>
#include <halm/generic/flash.h>
#include <halm/platform/lpc/spifi.h>
#include <halm/timer.h>
#include <assert.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define BUFFER_SIZE 2048
/*----------------------------------------------------------------------------*/
static bool memoryTestSequence(struct Interface *, struct Interface *);
static bool memoryTestSequenceZerocopy(struct Interface *);
static void onMemoryEvent(void *);
static void onTimerOverflow(void *);
/*----------------------------------------------------------------------------*/
static bool memoryTestSequence(struct Interface *spim,
    struct Interface *memory)
{
  static uint32_t address = 0;
  static bool useBlockErase = false;

  const uint32_t position = address;
  uint8_t buffer[BUFFER_SIZE];
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

  if (useBlockErase)
  {
    useBlockErase = false;
    res = ifSetParam(memory, IF_FLASH_ERASE_BLOCK, &position);
    if (res != E_OK)
      return false;
  }
  else
  {
    useBlockErase = true;
    res = ifSetParam(memory, IF_FLASH_ERASE_SECTOR, &position);
    if (res != E_OK)
      return false;
  }

  /* Verify erase */

  res = ifSetParam(memory, IF_POSITION, &position);
  if (res != E_OK)
    return false;

  memset(buffer, 0, sizeof(buffer));
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

  memset(buffer, 0, sizeof(buffer));
  count = ifRead(memory, buffer, sizeof(buffer));
  if (count != sizeof(buffer))
    return false;

  for (size_t i = 0; i < sizeof(buffer); ++i)
  {
    if (buffer[i] != (uint8_t)i)
      return false;
  }

  /* Verify memory mapping mode */

  const uint8_t * const region = spifiGetAddress((struct Spifi *)spim);

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
  static bool useBlockErase = false;

  const uint32_t position = address;
  uint8_t buffer[BUFFER_SIZE];
  uint32_t capacity = 0;
  uint32_t chunk = 0;
  uint32_t count;
  enum Result res;
  bool event = false;

  ifSetParam(memory, IF_ZEROCOPY, NULL);
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
    if (useBlockErase)
      res = ifSetParam(memory, IF_FLASH_ERASE_BLOCK, &position);
    else
      res = ifSetParam(memory, IF_FLASH_ERASE_SECTOR, &position);
    useBlockErase = !useBlockErase;

    if (res == E_OK || res == E_BUSY)
    {
      while (!event)
        barrier();

      event = false;
      res = ifGetParam(memory, IF_STATUS, NULL);
    }
  }

  /* Verify erase */

  if (res == E_OK)
    res = ifSetParam(memory, IF_POSITION, &position);
  if (res == E_OK)
  {
    memset(buffer, 0, sizeof(buffer));
    count = ifRead(memory, buffer, sizeof(buffer));

    if (count == sizeof(buffer))
    {
      while (!event)
        barrier();

      event = false;
      res = ifGetParam(memory, IF_STATUS, NULL);

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
      res = ifGetParam(memory, IF_STATUS, NULL);
    }
    else
      res = E_INTERFACE;
  }

  /* Verify program */

  if (res == E_OK)
    res = ifSetParam(memory, IF_POSITION, &position);
  if (res == E_OK)
  {
    memset(buffer, 0, sizeof(buffer));
    count = ifRead(memory, buffer, sizeof(buffer));

    if (count == sizeof(buffer))
    {
      while (!event)
        barrier();

      event = false;
      res = ifGetParam(memory, IF_STATUS, NULL);

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

  ifSetCallback(memory, NULL, NULL);
  ifSetParam(memory, IF_BLOCKING, NULL);
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
int main(void)
{
  bool event = false;

  boardSetupClockPll();

  const struct Pin blockingLed = pinInit(BOARD_LED_0);
  pinOutput(blockingLed, BOARD_LED_INV);
  const struct Pin zerocopyLed = pinInit(BOARD_LED_1);
  pinOutput(zerocopyLed, BOARD_LED_INV);

  struct Timer * const timer = boardSetupTimer();
  struct Interface * const spim = boardSetupSpim(NULL);

  const struct W25SPIMConfig w25Config = {
      .spim = spim,
      .shrink = false,
      .dtr = false,
      .xip = false
  };
  struct Interface * const memory = init(W25SPIM, &w25Config);
  assert(memory != NULL);

  timerSetOverflow(timer, timerGetFrequency(timer) * 5);
  timerSetCallback(timer, onTimerOverflow, &event);
  timerEnable(timer);

  while (1)
  {
    while (!event)
      barrier();
    event = false;

    pinSet(blockingLed);
    if (memoryTestSequence(spim, memory))
      pinReset(blockingLed);

    pinSet(zerocopyLed);
    if (memoryTestSequenceZerocopy(memory))
      pinReset(zerocopyLed);
  }

  return 0;
}
