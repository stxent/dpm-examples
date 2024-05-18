/*
 * m48x_dfu/shared/board.c
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/generic/flash.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/numicro/clocking.h>
#include <halm/platform/numicro/flash.h>
#include <halm/platform/numicro/gptimer.h>
#include <halm/platform/numicro/hsusb_device.h>
#include <halm/platform/numicro/pin_int.h>
#include <halm/platform/numicro/spim.h>
#include <halm/platform/numicro/usb_device.h>
#include <dpm/button.h>
#include <dpm/memory/w25_spim.h>
#include <dpm/usb/dfu_bridge.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
#define TRANSFER_SIZE 128

[[gnu::alias("boardSetupUsbFs")]] struct Entity *boardSetupUsb(void);
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};
/*----------------------------------------------------------------------------*/
void boardClockPostUpdate(struct Interface *spim)
{
  uint32_t frequency = clockFrequency(MainClock);
  enum Result res;

  while (frequency > 50000000)
    frequency /= 2;

  res = ifSetParam(spim, IF_RATE, &frequency);
  assert(res == E_OK);
  (void)res;
}
/*----------------------------------------------------------------------------*/
void boardResetClock(void)
{
  boardResetClockPartial();
  boardSetupClockInt();

  if (clockReady(SystemPll))
    clockDisable(SystemPll);

  if (clockReady(ExternalOsc))
    clockDisable(ExternalOsc);
}
/*----------------------------------------------------------------------------*/
void boardResetClockPartial(void)
{
  if (clockReady(UsbClock))
    clockDisable(UsbClock);
}
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void)
{
  static const struct ApbClockConfig apbClockConfigDirect = {
      .divisor = 1
  };
  static const struct ExtendedClockConfig mainClockConfigExt = {
      .divisor = 1,
      .source = CLOCK_EXTERNAL
  };

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(MainClock, &mainClockConfigExt);
  clockEnable(Apb0Clock, &apbClockConfigDirect);
  clockEnable(Apb1Clock, &apbClockConfigDirect);
}
/*----------------------------------------------------------------------------*/
void boardSetupClockInt(void)
{
  static const struct ApbClockConfig apbClockConfigDirect = {
      .divisor = 1
  };
  static const struct ExtendedClockConfig mainClockConfigInt = {
      .divisor = 1,
      .source = CLOCK_INTERNAL
  };

  clockEnable(MainClock, &mainClockConfigInt);
  clockEnable(Apb0Clock, &apbClockConfigDirect);
  clockEnable(Apb1Clock, &apbClockConfigDirect);
}
/*----------------------------------------------------------------------------*/
void boardSetupClockPll(void)
{
  static const struct ApbClockConfig apbClockConfigDivided = {
      .divisor = 2
  };
  static const struct ExtendedClockConfig mainClockConfigPll = {
      .divisor = 3,
      .source = CLOCK_PLL
  };
  static const struct PllConfig systemPllConfig = {
      .divisor = 1,
      .multiplier = 40,
      .source = CLOCK_EXTERNAL
  };

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(SystemPll, &systemPllConfig);
  while (!clockReady(SystemPll));

  clockEnable(Apb0Clock, &apbClockConfigDivided);
  clockEnable(Apb1Clock, &apbClockConfigDivided);
  clockEnable(MainClock, &mainClockConfigPll);
}
/*----------------------------------------------------------------------------*/
void boardSetupDefaultWQ(void)
{
  static const struct WorkQueueConfig wqConfig = {
      .size = 4
  };

  WQ_DEFAULT = init(WorkQueue, &wqConfig);
  assert(WQ_DEFAULT != NULL);
}
/*----------------------------------------------------------------------------*/
void boardSetupButtonPackage(struct ButtonPackage *package)
{
  static const struct PinIntConfig buttonEventConfig = {
      .pin = BOARD_BUTTON,
      .event = INPUT_FALLING,
      .pull = PIN_NOPULL
  };

  package->event = init(PinInt, &buttonEventConfig);
  assert(package->event != NULL);

  package->timer = boardSetupTimerAux0();
  timerSetOverflow(package->timer, timerGetFrequency(package->timer) / 100);

  const struct ButtonConfig buttonConfig = {
      .interrupt = package->event,
      .timer = package->timer,
      .pin = BOARD_BUTTON,
      .delay = 2,
      .level = false
  };
  package->button = init(Button, &buttonConfig);
  assert(package->button != NULL);
}
/*----------------------------------------------------------------------------*/
void boardSetupDfuPackage(struct DfuPackage *package, struct Interface *flash,
    struct FlashGeometry *geometry, size_t regions, size_t offset,
    void (*reset)(void))
{
  package->timer = boardSetupTimer();
  package->usb = boardSetupUsb();

  const struct DfuConfig dfuConfig = {
      .device = package->usb,
      .timer = package->timer,
      .transferSize = TRANSFER_SIZE
  };
  package->dfu = init(Dfu, &dfuConfig);
  assert(package->dfu != NULL);

  const struct DfuBridgeConfig bridgeConfig = {
      .device = package->dfu,
      .reset = reset,
      .flash = flash,
      .offset = offset,
      .geometry = geometry,
      .regions = regions,
      .writeonly = false
  };
  package->bridge = init(DfuBridge, &bridgeConfig);
  assert(package->bridge != NULL);
}
/*----------------------------------------------------------------------------*/
void boardSetupMemoryFlash(struct MemoryPackage *package)
{
  package->timer = NULL;
  package->spim = NULL;

  package->flash = init(Flash, &(struct FlashConfig){FLASH_BANK_0});
  assert(package->flash != NULL);

  package->offset = 0;
  package->regions = flashGetGeometry(package->flash, package->geometry,
      ARRAY_SIZE(package->geometry));
  assert(package->regions > 0);
}
/*----------------------------------------------------------------------------*/
void boardSetupMemoryNOR(struct MemoryPackage *package)
{
  package->timer = boardSetupTimerAux1();
  package->spim = boardSetupSpim(package->timer);

  const struct W25SPIMConfig w25Config = {
      .spim = package->spim,
      .strength = W25_DRV_75PCT,
      .dtr = true,
      .shrink = true,
      .xip = true
  };
  package->flash = init(W25SPIM, &w25Config);
  assert(package->flash != NULL);

  uint32_t capacity = 0;
  uint32_t sector = 0;

  ifGetParam(package->flash, IF_FLASH_SECTOR_SIZE, &sector);
  assert(sector > 0);
  ifGetParam(package->flash, IF_SIZE, &capacity);
  assert(capacity > 0);

  package->offset = 0;
  package->geometry[0].size = sector;
  package->geometry[0].count = capacity / sector;
  package->geometry[0].time = 50;
  package->regions = 1;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpim(struct Timer *timer)
{
  const struct SpimConfig spimConfig = {
      .timer = timer,
      .delay = 1,
      .rate = 4000000,
      .cs = PIN(PORT_C, 3),
      .io0 = PIN(PORT_C, 0),
      .io1 = PIN(PORT_C, 1),
      .io2 = PIN(PORT_C, 5),
      .io3 = PIN(PORT_C, 4),
      .sck = PIN(PORT_C, 2),
      .channel = 0
  };

  struct Interface * const interface = init(Spim, &spimConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .channel = 0
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimerAux0(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .channel = 1
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimerAux1(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .channel = 2
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Entity *boardSetupUsbFs(void)
{
  /* Clocks */
  static const struct ExtendedClockConfig usbClockConfig = {
      .divisor = 10,
      .source = CLOCK_PLL
  };

  /* Objects */
  static const struct UsbDeviceConfig fsUsbConfig = {
      .dm = PIN(PORT_A, 13),
      .dp = PIN(PORT_A, 14),
      .vbus = PIN(PORT_A, 12),
      .vid = 0x15A2,
      .pid = 0x0044,
      .channel = 0
  };

  assert(clockReady(SystemPll));
  clockEnable(UsbClock, &usbClockConfig);

  struct Entity * const usb = init(UsbDevice, &fsUsbConfig);
  assert(usb != NULL);
  return usb;
}
/*----------------------------------------------------------------------------*/
struct Entity *boardSetupUsbHs(void)
{
  static const struct UsbDeviceConfig hsUsbConfig = {
      .dm = PIN(PORT_HSUSB, PIN_HSUSB_DM),
      .dp = PIN(PORT_HSUSB, PIN_HSUSB_DP),
      .vbus = PIN(PORT_HSUSB, PIN_HSUSB_VBUS),
      .vid = 0x15A2,
      .pid = 0x0044,
      .channel = 0
  };

  assert(clockReady(ExternalOsc));

  struct Entity * const usb = init(HsUsbDevice, &hsUsbConfig);
  assert(usb != NULL);
  return usb;
}
