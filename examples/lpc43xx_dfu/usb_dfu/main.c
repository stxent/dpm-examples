/*
 * lpc43xx_dfu/usb_dfu/main.c
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/core/cortex/nvic.h>
#include <halm/generic/work_queue.h>
#include <halm/interrupt.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/usb/usb.h>
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
extern unsigned long _srom;
extern unsigned long _erom;

static struct ClockSettings sharedClockSettings
    __attribute__((section(".shared")));

struct Board instance;
/*----------------------------------------------------------------------------*/
static void boardInit(struct Board *board)
{
  board->ind0 = pinInit(BOARD_USB_IND0);
  pinOutput(board->ind0, !BOARD_LED_INV);
  board->ind1 = pinInit(BOARD_USB_IND1);
  pinOutput(board->ind1, BOARD_LED_INV);

  boardSetupClockPll();
  boardSetupMemoryFlash(&board->memoryPackage);
  storeClockSettings(&sharedClockSettings);

  board->memoryPackage.offset = (uintptr_t)&_erom - (uintptr_t)&_srom;

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
  /* Flash driver should be left in initialized state */

  boardResetClock();
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
  const uint32_t *table = flashGetAddress(board->memoryPackage.flash);

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
