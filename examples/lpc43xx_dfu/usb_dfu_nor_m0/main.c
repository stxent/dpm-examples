/*
 * lpc43xx_dfu/usb_dfu_nor_m0/main.c
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/generic/spim.h>
#include <halm/generic/work_queue.h>
#include <halm/interrupt.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/system.h>
#include <halm/timer.h>
#include <halm/usb/usb.h>
#include <halm/usb/usb_langid.h>
#include <dpm/memory/w25_spim.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
struct Board
{
  struct Pin ind0;
  struct Pin ind1;

  struct TimerPackage timerPackage;
  struct MemoryPackage memoryPackage;
  struct ButtonPackage buttonPackage;
  struct DfuPackage dfuPackage;

  bool running;
};
/*----------------------------------------------------------------------------*/
static void boardInit(struct Board *);
static void customStringHeader(const void *, enum UsbLangId,
    struct UsbDescriptor *, void *);
static void customStringWrapper(const void *, enum UsbLangId,
    struct UsbDescriptor *, void *);
static void onButtonPressed(void *);
static void onResetRequested(void);
static void startFirmwareTask(void *);
static void stopFirmwareTask(void *);
/*----------------------------------------------------------------------------*/
[[gnu::section(".shared")]] static struct ClockSettings sharedClockSettings;
static const char productStringEn[] = "LPC43xx M0 DFU for NOR";

struct Board instance;
/*----------------------------------------------------------------------------*/
static void boardInit(struct Board *board)
{
  board->ind0 = pinInit(BOARD_USB_IND0);
  pinOutput(board->ind0, !BOARD_LED_INV);
  board->ind1 = pinInit(BOARD_USB_IND1);
  pinOutput(board->ind1, BOARD_LED_INV);

  boardSetupClockPll();
  boardClockPostUpdate();
  boardSetupMemoryNOR(&board->memoryPackage);
  storeClockSettings(&sharedClockSettings);

  board->memoryPackage.offset = 131072;
  board->running = false;

  boardSetupTimerPackage(&board->timerPackage);
  boardSetupButtonPackage(&board->buttonPackage, board->timerPackage.factory);
  boardSetupDfuPackage(&board->dfuPackage, board->timerPackage.factory,
      board->memoryPackage.flash, board->memoryPackage.geometry,
      board->memoryPackage.regions, board->memoryPackage.offset,
      onResetRequested);

  timerEnable(board->timerPackage.timer);
  interruptSetCallback(board->buttonPackage.button, onButtonPressed, board);
  interruptEnable(board->buttonPackage.button);
}
/*----------------------------------------------------------------------------*/
static void customStringHeader(const void *, enum UsbLangId,
    struct UsbDescriptor *header, void *payload)
{
  usbStringHeader(header, payload, LANGID_ENGLISH_US);
}
/*----------------------------------------------------------------------------*/
static void customStringWrapper(const void *argument, enum UsbLangId,
    struct UsbDescriptor *header, void *payload)
{
  usbStringWrap(header, payload, argument);
}
/*----------------------------------------------------------------------------*/
static void onButtonPressed(void *argument)
{
  struct Board * const board = argument;

  if (board->running)
    wqAdd(WQ_DEFAULT, stopFirmwareTask, argument);
  else
    wqAdd(WQ_DEFAULT, startFirmwareTask, argument);
}
/*----------------------------------------------------------------------------*/
static void onResetRequested(void)
{
  if (instance.running)
    wqAdd(WQ_DEFAULT, stopFirmwareTask, &instance);

  wqAdd(WQ_DEFAULT, startFirmwareTask, &instance);
}
/*----------------------------------------------------------------------------*/
static void startFirmwareTask(void *argument)
{
  struct Board * const board = argument;
  uintptr_t address;

  ifGetParam(board->memoryPackage.spifi, IF_SPIM_MEMORY_MAPPED_ADDRESS,
      &address);

  board->running = true;
  pinWrite(board->ind1, !BOARD_LED_INV);

  w25MemoryMappingEnable((struct W25SPIM *)board->memoryPackage.flash);

  sysCoreM0AppRemap(address + board->memoryPackage.offset);
  sysClockEnable(CLK_M4_M0APP);
  sysResetDisable(RST_M0APP);
}
/*----------------------------------------------------------------------------*/
static void stopFirmwareTask(void *argument)
{
  struct Board * const board = argument;

  sysResetEnable(RST_M0APP);
  w25MemoryMappingDisable((struct W25SPIM *)board->memoryPackage.flash);

  board->running = false;
  pinWrite(board->ind1, BOARD_LED_INV);
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  boardSetupDefaultWQ();
  boardInit(&instance);

  usbDevStringAppend(instance.dfuPackage.usb, usbStringBuild(
      customStringHeader, 0, USB_STRING_HEADER, 0));
  usbDevStringAppend(instance.dfuPackage.usb, usbStringBuild(
      customStringWrapper, productStringEn, USB_STRING_PRODUCT, 0));
  usbDevSetConnected(instance.dfuPackage.usb, true);

  wqStart(WQ_DEFAULT);
  return 0;
}
