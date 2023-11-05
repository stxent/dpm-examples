/*
 * lpc17xx_default/usb_dfu/main.c
 * Copyright (C) 2022 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include <dpm/usb/dfu_bridge.h>
#include <halm/core/cortex/nvic.h>
#include <halm/generic/flash.h>
#include <halm/generic/work_queue.h>
#include <halm/pin.h>
#include <halm/platform/lpc/backup_domain.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/flash.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/usb_device.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
#define BOOT_PIN        PIN(2, 10)
#define BOOT_PIN_INVERT 0

#define FIRMWARE_OFFSET 0x4000
#define MAGIC_WORD      0x3A84508FUL
#define TRANSFER_SIZE   128
/*----------------------------------------------------------------------------*/
static inline void fwRequestClear(void);
static bool isDfuRequested(void);
static void onResetRequested(void);
static void setupClock(void);
static void startFirmware(void);
/*----------------------------------------------------------------------------*/
static struct GpTimerConfig timerConfig = {
    .frequency = 1000,
    .channel = 0
};

static const struct UsbDeviceConfig usbConfig = {
    .dm = PIN(0, 30),
    .dp = PIN(0, 29),
    .connect = PIN(2, 9),
    .vbus = PIN(1, 30),
    .vid = 0x15A2,
    .pid = 0x0044,
    .channel = 0
};

static const struct WorkQueueConfig workQueueConfig = {
    .size = 2
};
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
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
static inline void fwRequestClear(void)
{
  *(uint32_t *)backupDomainAddress() = 0;
}
/*----------------------------------------------------------------------------*/
static bool isDfuRequested(void)
{
  const struct Pin bootPin = pinInit(BOOT_PIN);
  pinInput(bootPin);
  pinSetPull(bootPin, BOOT_PIN_INVERT ? PIN_PULLUP : PIN_PULLDOWN);

  const bool fwRequest = *(const uint32_t *)backupDomainAddress() == MAGIC_WORD;
  const bool pinRequest = pinRead(bootPin) ^ BOOT_PIN_INVERT;

  return fwRequest || pinRequest;
}
/*----------------------------------------------------------------------------*/
static void onResetRequested(void)
{
  fwRequestClear();
  nvicResetCore();
}
/*----------------------------------------------------------------------------*/
static void setupClock(void)
{
  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(SystemPll, &sysPllConfig);
  while (!clockReady(SystemPll));

  clockEnable(MainClock, &mainClockConfig);

  clockEnable(UsbClock, &mainClockConfig);
  while (!clockReady(UsbClock));
}
/*----------------------------------------------------------------------------*/
static void startFirmware(void)
{
  const uint32_t * const table = (const uint32_t *)FIRMWARE_OFFSET;

  if (((table[0] >= 0x10000000 && table[0] <= 0x10008000)
          || (table[0] >= 0x2007C000 && table[0] <= 0x20084000))
      && table[1] <= 0x00080000)
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
  if (!isDfuRequested())
    startFirmware();

  setupClock();

  struct FlashGeometry layout[2];
  struct Interface * const flash = init(Flash, NULL);
  assert(flash != NULL);
  const size_t regions = flashGetGeometry(flash, layout, ARRAY_SIZE(layout));
  assert(regions != 0);

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);

  struct Entity * const usb = init(UsbDevice, &usbConfig);
  assert(usb != NULL);

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
      .writeonly = false
  };
  struct DfuBridge * const bridge = init(DfuBridge, &bridgeConfig);
  assert(bridge != NULL);
  (void)bridge;

  /* Initialize Work Queue */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT != NULL);

  /* Start USB enumeration and event loop */
  usbDevSetConnected(usb, true);
  wqStart(WQ_DEFAULT);

  return 0;
}
