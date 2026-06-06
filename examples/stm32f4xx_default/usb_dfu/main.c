/*
 * stm32f4xx_default/usb_dfu/main.c
 * Copyright (C) 2026 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <dpm/usb/dfu_bridge.h>
#include <halm/core/cortex/nvic.h>
#include <halm/generic/flash.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/stm32/backup_domain.h>
#include <halm/platform/stm32/flash.h>
#include <halm/platform/stm32/usb_device.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
#define FIRMWARE_OFFSET 0x8000
#define MAGIC_WORD      0x3A84508FUL
#define TRANSFER_SIZE   128
/*----------------------------------------------------------------------------*/
static inline void fwRequestClear(void);
static bool isDfuRequested(void);
static void onResetRequested(void);
static void startFirmware(void);
/*----------------------------------------------------------------------------*/
static inline void fwRequestClear(void)
{
  backupDomainUnlock();
  *(uint32_t *)backupDomainAddress() = 0;
  backupDomainLock();
}
/*----------------------------------------------------------------------------*/
static bool isDfuRequested(void)
{
  const struct Pin bootPin = pinInit(BOARD_BUTTON);
  pinInput(bootPin);
  pinSetPull(bootPin, BOARD_BUTTON_INV ? PIN_PULLUP : PIN_PULLDOWN);

  const bool fwRequest = *(const uint32_t *)backupDomainAddress() == MAGIC_WORD;
  const bool pinRequest = pinRead(bootPin) ^ BOARD_BUTTON_INV;

  return fwRequest || pinRequest;
}
/*----------------------------------------------------------------------------*/
static void onResetRequested(void)
{
  fwRequestClear();
  nvicResetCore();
}
/*----------------------------------------------------------------------------*/
static void startFirmware(void)
{
  const uint32_t * const table = (const uint32_t *)FIRMWARE_OFFSET;

  if ((table[0] >= 0x20000000 && table[0] <= 0x20020000)
      && (table[1] >= 0x08000000 && table[1] < 0x08100000))
  {
    void (*resetVector)(void) = (void (*)(void))table[1];

    nvicSetVectorTableOffset((uint32_t)table);
    __setMainStackPointer(table[0]);
    resetVector();
  }
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  backupDomainEnable();
  if (!isDfuRequested())
    startFirmware();

  boardSetupClockPll();
  boardSetupDefaultWQ();

  struct Interface * const flash = init(Flash, NULL);
  assert(flash != NULL);

  struct FlashGeometry layout[3];
  const size_t regions = flashGetGeometry(flash, layout, ARRAY_SIZE(layout));
  assert(regions != 0);

  struct Timer * const timer = boardSetupTimer();
  struct Usb * const usb = boardSetupUsb();

  const struct DfuConfig dfuConfig = {
      .device = usb,
      .timer = timer,
      .transferSize = TRANSFER_SIZE
  };
  struct Dfu * const dfu = init(Dfu, &dfuConfig);
  assert(dfu != NULL);

  const struct DfuBridgeConfig bridgeConfig = {
      .device = dfu,
      .reset = onResetRequested,
      .flash = flash,
      .offset = FIRMWARE_OFFSET,
      .geometry = layout,
      .regions = regions,
      .chunk = 4096,
      .writeonly = false
  };
  struct DfuBridge * const bridge = init(DfuBridge, &bridgeConfig);
  assert(bridge != NULL);
  (void)bridge;

  /* Start USB enumeration and event loop */
  usbDevSetConnected(usb, true);
  wqStart(WQ_DEFAULT);

  return 0;
}
