/*
 * lpc43xx_dfu/usb_dfu_nor/main.c
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/core/cortex/nvic.h>
#include <halm/generic/spim.h>
#include <halm/generic/work_queue.h>
#include <halm/interrupt.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/timer.h>
#include <halm/usb/usb.h>
#include <halm/usb/usb_langid.h>
#include <dpm/memory/w25_spim.h>
#include <xcore/asm.h>
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
};
/*----------------------------------------------------------------------------*/
static void boardInit(struct Board *);
static void boardDeinit(struct Board *);
static void customStringHeader(const void *, enum UsbLangId,
    struct UsbDescriptor *, void *);
static void customStringWrapper(const void *, enum UsbLangId,
    struct UsbDescriptor *, void *);
static void onButtonPressed(void *);
static void onResetRequested(void);
static void startFirmware(struct Board *);
static void startFirmwareTask(void *);
/*----------------------------------------------------------------------------*/
extern unsigned long _stext;

[[gnu::section(".shared")]] static struct ClockSettings sharedClockSettings;
static const char productStringEn[] = "LPC43xx M4 DFU for NOR";

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
  storeClockSettings(&sharedClockSettings);

#ifdef CONFIG_FIRST_STAGE
  board->memoryPackage.offset = 0;
#else
  board->memoryPackage.offset = 131072;
#endif

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
static void boardDeinit(struct Board *board)
{
  usbDevSetConnected(board->dfuPackage.usb, false);

  interruptDisable(board->buttonPackage.button);
  timerDisable(board->timerPackage.timer);
  deinit(board->dfuPackage.bridge);
  deinit(board->dfuPackage.dfu);
  deinit(board->dfuPackage.usb);
  deinit(board->dfuPackage.timer);
  deinit(board->buttonPackage.button);
  deinit(board->buttonPackage.timer);
  deinit(board->buttonPackage.event);
  deinit(board->timerPackage.factory);
  deinit(board->timerPackage.timer);
  deinit(board->memoryPackage.flash);
  /* SPIFI driver should be left in initialized state */

  boardResetClockPartial();
  pinWrite(board->ind0, BOARD_LED_INV);
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
  ifGetParam(board->memoryPackage.spifi,
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
  boardClockPostUpdate();

  boardDeinit(board);
  startFirmware(board);
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
