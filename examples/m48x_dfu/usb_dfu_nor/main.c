/*
 * m48x_dfu/usb_dfu_nor/main.c
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/core/cortex/nvic.h>
#include <halm/generic/spim.h>
#include <halm/generic/work_queue.h>
#include <halm/interrupt.h>
#include <halm/usb/usb.h>
#include <dpm/memory/w25_spim.h>
#include <xcore/asm.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
struct Board
{
  struct Pin ind0;
  struct Pin ind1;

  struct MemoryPackage memoryPackage;
  struct ButtonPackage buttonPackage;
  struct DfuPackage dfuPackage;
};
/*----------------------------------------------------------------------------*/
static void boardInit(struct Board *);
static void boardDeinit(struct Board *);
static void onButtonPressed(void *);
static void onResetRequested(void);
static void startFirmware(struct Board *);
static void startFirmwareTask(void *);
/*----------------------------------------------------------------------------*/
struct Board instance;
/*----------------------------------------------------------------------------*/
static void boardInit(struct Board *board)
{
  board->ind0 = pinInit(BOARD_USB_IND0);
  pinOutput(board->ind0, !BOARD_LED_INV);
  board->ind1 = pinInit(BOARD_USB_IND1);
  pinOutput(board->ind1, BOARD_LED_INV);

  boardSetupClockPll();
  boardSetupMemoryNOR(&board->memoryPackage);

#ifdef CONFIG_FIRST_STAGE
  board->memoryPackage.offset = 0;
#else
  board->memoryPackage.offset = 131072;
#endif

  boardSetupButtonPackage(&board->buttonPackage);
  boardSetupDfuPackage(&board->dfuPackage, board->memoryPackage.flash,
      board->memoryPackage.geometry, board->memoryPackage.regions,
      board->memoryPackage.offset, onResetRequested);

  interruptSetCallback(board->buttonPackage.button, onButtonPressed, board);
  interruptEnable(board->buttonPackage.button);
}
/*----------------------------------------------------------------------------*/
static void boardDeinit(struct Board *board)
{
  usbDevSetConnected(board->dfuPackage.usb, false);

  deinit(board->dfuPackage.bridge);
  deinit(board->dfuPackage.dfu);
  deinit(board->dfuPackage.usb);
  deinit(board->dfuPackage.timer);
  deinit(board->buttonPackage.button);
  deinit(board->buttonPackage.timer);
  deinit(board->buttonPackage.event);
  deinit(board->memoryPackage.flash);
  deinit(board->memoryPackage.timer);
  /* SPIM driver should be left in initialized state */

  boardResetClockPartial();
  pinWrite(board->ind0, BOARD_LED_INV);
}
/*----------------------------------------------------------------------------*/
static void onButtonPressed(void *argument)
{
  wqAdd(WQ_DEFAULT, startFirmwareTask, argument);
}
/*----------------------------------------------------------------------------*/
static void onResetRequested(void)
{
  wqAdd(WQ_DEFAULT, startFirmwareTask, &instance);
}
/*----------------------------------------------------------------------------*/
static void startFirmware(struct Board *board)
{
  uintptr_t address = 0;
  ifGetParam(board->memoryPackage.spim,
      IF_SPIM_MEMORY_MAPPED_ADDRESS, &address);

  const uint32_t *table = (const uint32_t *)address;

  table += board->memoryPackage.offset / sizeof(uint32_t);
  void (*resetVector)(void) = (void (*)(void))table[1];

  nvicSetVectorTableOffset((uint32_t)table);
  __setMainStackPointer(table[0]);
  resetVector();
}
/*----------------------------------------------------------------------------*/
static void startFirmwareTask(void *argument)
{
  struct Board * const board = argument;

  w25MemoryMappingEnable((struct W25SPIM *)board->memoryPackage.flash);
  boardClockPostUpdate(board->memoryPackage.spim);

  boardDeinit(board);
  startFirmware(board);
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  boardSetupDefaultWQ();
  boardInit(&instance);

  usbDevSetConnected(instance.dfuPackage.usb, true);
  wqStart(WQ_DEFAULT);

  return 0;
}
