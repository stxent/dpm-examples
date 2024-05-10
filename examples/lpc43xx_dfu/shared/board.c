/*
 * lpc43xx_dfu/shared/board.c
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/core/cortex/systick.h>
#include <halm/delay.h>
#include <halm/generic/flash.h>
#include <halm/generic/ram_proxy.h>
#include <halm/generic/timer_factory.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/flash.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/pin_int.h>
#include <halm/platform/lpc/spifi.h>
#include <halm/platform/lpc/system.h>
#include <halm/platform/lpc/usb_device.h>
#include <dpm/button.h>
#include <dpm/memory/w25_spim.h>
#include <dpm/usb/dfu_bridge.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
#define TRANSFER_SIZE 128

[[gnu::alias("boardSetupUsb0")]] struct Entity *boardSetupUsb(void);
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};
/*----------------------------------------------------------------------------*/
void boardClockPostUpdate(void)
{
  static const struct GenericDividerConfig divConfig = {
      .divisor = 2,
      .source = CLOCK_PLL
  };

  clockEnable(DividerD, &divConfig);
  while (!clockReady(DividerD));

  /* Switch to high-frequency clock */
  clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_IDIVD});
  while (!clockReady(SpifiClock));
}
/*----------------------------------------------------------------------------*/
void boardResetClock(void)
{
  boardResetClockPartial();

  clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_INTERNAL});

  if (clockReady(SystemPll))
    clockDisable(SystemPll);

  if (clockReady(ExternalOsc))
    clockDisable(ExternalOsc);

  /* Flash latency should be reset to exit correctly from power-down modes */
  sysFlashLatencyReset();
}
/*----------------------------------------------------------------------------*/
void boardResetClockPartial(void)
{
  if (clockReady(Usb0Clock))
  {
    clockDisable(Usb0Clock);
    clockDisable(UsbPll);
  }

  if (clockReady(Usb1Clock))
  {
    clockDisable(Usb1Clock);
    clockDisable(DividerA);
    clockDisable(AudioPll);
  }
}
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void)
{
  clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_INTERNAL});

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_EXTERNAL});
}
/*----------------------------------------------------------------------------*/
void boardSetupClockPll(void)
{
  static const struct GenericDividerConfig divConfig = {
      .divisor = 2,
      .source = CLOCK_PLL
  };
  static const struct PllConfig systemPllConfig = {
      .divisor = 1,
      .multiplier = 17,
      .source = CLOCK_EXTERNAL
  };

  clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_INTERNAL});

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(SystemPll, &systemPllConfig);
  while (!clockReady(SystemPll));

  /* Make a PLL clock divided by 2 for base clock ramp up */
  clockEnable(DividerA, &divConfig);
  while (!clockReady(DividerA));

  clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_IDIVA});
  udelay(50);
  clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_PLL});

  /* Base clock is ready, temporary clock divider is not needed anymore */
  clockDisable(DividerA);
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
void boardSetupButtonPackage(struct ButtonPackage *package,
    struct TimerFactory *factory)
{
  static const struct PinIntConfig buttonEventConfig = {
      .pin = BOARD_BUTTON,
      .event = INPUT_FALLING,
      .pull = PIN_PULLUP
  };

  package->event = init(PinInt, &buttonEventConfig);
  assert(package->event != NULL);

  package->timer = timerFactoryCreate(factory);
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
void boardSetupDfuPackage(struct DfuPackage *package,
    struct TimerFactory *factory, struct Interface *flash,
    struct FlashGeometry *geometry, size_t regions, size_t offset,
    void (*reset)(void))
{
  package->timer = timerFactoryCreate(factory);
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
void boardSetupTimerPackage(struct TimerPackage *package)
{
  package->timer = boardSetupTimer();
  assert(package->timer != NULL);
  timerSetOverflow(package->timer, timerGetFrequency(package->timer) / 1000);

  const struct TimerFactoryConfig timerFactoryConfig = {
      .timer = package->timer
  };
  package->factory = init(TimerFactory, &timerFactoryConfig);
  assert(package->factory != NULL);
}
/*----------------------------------------------------------------------------*/
void boardSetupMemoryFlash(struct MemoryPackage *package)
{
  package->spifi = NULL;

  package->flash = init(Flash, &(struct FlashConfig){FLASH_BANK_A});
  assert(package->flash != NULL);

  package->offset = 0;
  package->regions = flashGetGeometry(package->flash, package->geometry,
      ARRAY_SIZE(package->geometry));
  assert(package->regions > 0);
}
/*----------------------------------------------------------------------------*/
void boardSetupMemoryFlashB(struct MemoryPackage *package)
{
  package->spifi = NULL;

  package->flash = init(Flash, &(struct FlashConfig){FLASH_BANK_B});
  assert(package->flash != NULL);

  package->offset = 0;
  package->regions = flashGetGeometry(package->flash, package->geometry,
      ARRAY_SIZE(package->geometry));
  assert(package->regions > 0);
}
/*----------------------------------------------------------------------------*/
void boardSetupMemoryNOR(struct MemoryPackage *package)
{
  package->spifi = boardSetupSpim();

  const struct W25SPIMConfig w25Config = {
      .spim = package->spifi,
      .strength = W25_DRV_75PCT,
      .dtr = false,
      .shrink = true,
      .xip = false
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
void boardSetupMemorySRAM(struct MemoryPackage *package, void *arena,
    size_t size)
{
  const struct RamProxyConfig ramConfig = {
      .arena = arena,
      .capacity = size,
      .granule = 0
  };
  package->flash = init(RamProxy, &ramConfig);
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
struct Interface *boardSetupSpim(void)
{
  /* Objects */
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

  clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_INTERNAL});
  while (!clockReady(SpifiClock));

  struct Interface * const interface = init(Spifi, &spifiConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer(void)
{
  struct Timer * const timer = init(SysTick, NULL);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Entity *boardSetupUsb0(void)
{
  /* Clocks */
  static const struct PllConfig usbPllConfig = {
      .divisor = 1,
      .multiplier = 40,
      .source = CLOCK_EXTERNAL
  };

  /* Objetcs */
  static const struct UsbDeviceConfig usb0Config = {
      .dm = PIN(PORT_USB, PIN_USB0_DM),
      .dp = PIN(PORT_USB, PIN_USB0_DP),
      .connect = 0,
      .vbus = PIN(PORT_USB, PIN_USB0_VBUS),
      .vid = 0x15A2,
      .pid = 0x0044,
      .channel = 0
  };

  clockEnable(UsbPll, &usbPllConfig);
  while (!clockReady(UsbPll));

  clockEnable(Usb0Clock, &(struct GenericClockConfig){CLOCK_USB_PLL});
  while (!clockReady(Usb0Clock));

  struct Entity * const usb = init(UsbDevice, &usb0Config);
  assert(usb != NULL);
  return usb;
}
/*----------------------------------------------------------------------------*/
struct Entity *boardSetupUsb1(void)
{
  /* Clocks */
  static const struct PllConfig audioPllConfig = {
      .source = CLOCK_EXTERNAL,
      .divisor = 4,
      .multiplier = 40
  };
  static const struct GenericDividerConfig divConfig = {
      .divisor = 2,
      .source = CLOCK_AUDIO_PLL
  };

  /* Objects */
  static const struct UsbDeviceConfig usb1Config = {
      .dm = PIN(PORT_USB, PIN_USB1_DM),
      .dp = PIN(PORT_USB, PIN_USB1_DP),
      .connect = 0,
      .vbus = PIN(PORT_2, 5),
      .vid = 0x15A2,
      .pid = 0x0044,
      .channel = 1
  };

  /* Make 120 MHz clock on AUDIO PLL */
  if (!clockReady(AudioPll))
  {
    clockEnable(AudioPll, &audioPllConfig);
    while (!clockReady(AudioPll));
  }

  /* Make 60 MHz clock required for USB1 */
  clockEnable(DividerA, &divConfig);
  while (!clockReady(DividerA));

  clockEnable(Usb1Clock, &(struct GenericClockConfig){CLOCK_IDIVA});
  while (!clockReady(Usb1Clock));

  struct Entity * const usb = init(UsbDevice, &usb1Config);
  assert(usb != NULL);
  return usb;
}
