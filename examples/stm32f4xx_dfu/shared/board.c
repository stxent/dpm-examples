/*
 * stm32f4xx_dfu/shared/board.c
 * Copyright (C) 2026 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/generic/ram_proxy.h>
#include <halm/generic/timer_factory.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/stm32/clocking.h>
#include <halm/platform/stm32/exti.h>
#include <halm/platform/stm32/flash.h>
#include <halm/platform/stm32/fsmc_sram.h>
#include <halm/platform/stm32/gptimer.h>
#include <halm/platform/stm32/usb_device.h>
#include <dpm/button.h>
#include <dpm/usb/dfu_bridge.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
#define TRANSFER_SIZE 128
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
    .frequency = 8000000
};

static const struct MainClockConfig mainClockConfig = {
    .divisor = 1,
    .range = VR_2V7_3V6
};
/*----------------------------------------------------------------------------*/
void boardResetClock(void)
{
  clockEnable(MainClock, &(struct SystemClockConfig){CLOCK_INTERNAL});

  if (clockReady(MainPll))
    clockDisable(MainPll);

  if (clockReady(ExternalOsc))
    clockDisable(ExternalOsc);
}
/*----------------------------------------------------------------------------*/
void boardSetupClockPll(void)
{
  static const struct PllConfig mainPllConfig = {
      .divisor = 2,
      .multiplier = 42,
      .source = CLOCK_EXTERNAL
  };

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(MainPll, &mainPllConfig);
  while (!clockReady(MainPll));

  clockEnable(Apb1Clock, &(struct BusClockConfig){4});
  clockEnable(Apb2Clock, &(struct BusClockConfig){2});
  clockEnable(SystemClock, &(struct SystemClockConfig){CLOCK_PLL});

  clockEnable(MainClock, &mainClockConfig);
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
  static const struct ExtiConfig buttonIntConfig = {
      .pin = BOARD_BUTTON,
      .event = BOARD_BUTTON_INV ? INPUT_FALLING : INPUT_RISING,
      .pull = BOARD_BUTTON_INV ? PIN_PULLUP : PIN_PULLDOWN
  };

  package->event = init(Exti, &buttonIntConfig);
  assert(package->event != NULL);

  package->timer = timerFactoryCreate(factory);
  timerSetOverflow(package->timer, timerGetFrequency(package->timer) / 100);

  const struct ButtonConfig buttonConfig = {
      .interrupt = package->event,
      .timer = package->timer,
      .pin = BOARD_BUTTON,
      .delay = 2,
      .level = BOARD_BUTTON_INV ? false : true
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
      .chunk = 0,
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

  const struct TimerFactoryConfig timerFactoryConfig = {
      .timer = package->timer
  };
  package->factory = init(TimerFactory, &timerFactoryConfig);
  assert(package->factory != NULL);
  timerSetOverflow(package->factory,
      timerGetFrequency(package->factory) / 1000);
}
/*----------------------------------------------------------------------------*/
void boardSetupMemoryESRAM(struct MemoryPackage *package)
{
  static const struct FsmcSramConfig fsmcSramConfig = {
      .timings = {
          .oe = 0,
          .rd = 70,
          .we = 30,
          .wr = 70
      },

      .width = {
          .address = 19,
          .data = 16
      },

      .speed = PIN_SLEW_FAST,
      .subbank = 2,
      .useWriteEnable = true
  };

  package->lower = init(FsmcSram, &fsmcSramConfig);
  assert(package->lower != NULL);

  const struct RamProxyConfig ramConfig = {
      .arena = fsmcSramAddress((const struct FsmcSram *)package->lower),
      .capacity = fsmcSramSize((const struct FsmcSram *)package->lower),
      .granule = 0
  };
  package->upper = init(RamProxy, &ramConfig);
  assert(package->upper != NULL);

  uint32_t capacity = 0;
  uint32_t sector = 0;

  ifGetParam(package->upper, IF_FLASH_SECTOR_SIZE, &sector);
  assert(sector > 0);
  ifGetParam(package->upper, IF_SIZE, &capacity);
  assert(capacity > 0);

  package->offset = 0;
  package->geometry[0].size = sector;
  package->geometry[0].count = capacity / sector;
  package->geometry[0].time = 50;
  package->regions = 1;
}
/*----------------------------------------------------------------------------*/
void boardSetupMemoryFlash(struct MemoryPackage *package)
{
  static const struct FlashConfig flashConfig = {
      .voltage = VR_2V7_3V6
  };

  package->lower = NULL;

  package->upper = init(Flash, &flashConfig);
  assert(package->upper != NULL);

  package->offset = 0;
  package->regions = flashGetGeometry(package->upper, package->geometry,
      ARRAY_SIZE(package->geometry));
  assert(package->regions > 0);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 10000,
      .channel = TIM6
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Usb *boardSetupUsb(void)
{
  static const struct UsbDeviceConfig usbConfig = {
      .dm = PIN(PORT_A, 11),
      .dp = PIN(PORT_A, 12),
      .vid = 0x15A2,
      .pid = 0x0044,
      .channel = 0
  };

  struct Usb * const usb = init(UsbDevice, &usbConfig);
  assert(usb != NULL);
  return usb;
}
