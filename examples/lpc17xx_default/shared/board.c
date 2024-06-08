/*
 * lpc17xx_default/shared/board.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <dpm/platform/lpc/irda.h>
#include <dpm/platform/lpc/memory_bus_dma.h>
#include <dpm/platform/lpc/memory_bus_gpio.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/fast_gpio_bus.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/i2c.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/platform/lpc/one_wire_ssp.h>
#include <halm/platform/lpc/pin_int.h>
#include <halm/platform/lpc/serial.h>
#include <halm/platform/lpc/spi_dma.h>
#include <halm/platform/lpc/usb_device.h>
#include <halm/platform/lpc/wdt.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
[[gnu::alias("boardSetupDisplayBusDma")]]
    struct Interface *boardSetupDisplayBus(void);

[[gnu::alias("boardSetupSpi1")]] struct Interface *boardSetupSpiDisplay(void);

[[gnu::alias("boardSetupSpi1")]] struct Interface *boardSetupSpi(void);

[[gnu::alias("boardSetupTimer3")]] struct Timer *boardSetupTimer(void);
[[gnu::alias("boardSetupTimer2")]] struct Timer *boardSetupTimerAux0(void);
[[gnu::alias("boardSetupTimer1")]] struct Timer *boardSetupTimerAux1(void);
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};
/*----------------------------------------------------------------------------*/
DECLARE_WQ_IRQ(WQ_LP, SPI_ISR)
/*----------------------------------------------------------------------------*/
void boardResetClock(void)
{
  static const struct GenericClockConfig mainClockConfigInt = {
      .source = CLOCK_INTERNAL
  };

  clockEnable(MainClock, &mainClockConfigInt);

  if (clockReady(UsbPll))
    clockDisable(UsbPll);

  if (clockReady(SystemPll))
    clockDisable(SystemPll);

  if (clockReady(ExternalOsc))
    clockDisable(ExternalOsc);
}
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void)
{
  static const struct GenericClockConfig mainClockConfigExt = {
      .source = CLOCK_EXTERNAL
  };

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(MainClock, &mainClockConfigExt);
}
/*----------------------------------------------------------------------------*/
void boardSetupClockPll(void)
{
  static const struct PllConfig sysPllConfig = {
      .divisor = 4,
      .multiplier = 32,
      .source = CLOCK_EXTERNAL
  };
  static const struct GenericClockConfig mainClockConfigPll = {
      .source = CLOCK_PLL
  };

  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(SystemPll, &sysPllConfig);
  while (!clockReady(SystemPll));

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
void boardSetupLowPriorityWQ(void)
{
  static const struct WorkQueueIrqConfig wqIrqConfig = {
      .size = 4,
      .irq = SPI_IRQ,
      .priority = 0
  };

  WQ_LP = init(WorkQueueIrq, &wqIrqConfig);
  assert(WQ_LP != NULL);
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardSetupButton(void)
{
  static const struct PinIntConfig buttonIntConfig = {
      .pin = BOARD_BUTTON,
      .event = BOARD_BUTTON_INV ? INPUT_FALLING : INPUT_RISING,
      .pull = BOARD_BUTTON_INV ? PIN_PULLUP : PIN_PULLDOWN
  };

  struct Interrupt * const interrupt = init(PinInt, &buttonIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupDisplayBusDma(void)
{
  static const PinNumber busPins[] = {
      PIN(2, 0), PIN(2, 1), PIN(2, 2), PIN(2, 3),
      PIN(2, 4), PIN(2, 5), PIN(2, 6), PIN(2, 7),
      0
  };
  static const struct MemoryBusDmaConfig busConfig = {
      .pins = busPins,
      .size = 32768,
      .cycle = 16,
      .priority = 0,

      .clock = {
          .leading = PIN(1, 28),
          .trailing = PIN(1, 29),
          .channel = 0,
          .dma = 1,
          .inversion = false,
          .swap = true
      },
      .control = {
          .capture = PIN(1, 19),
          .leading = PIN(1, 22),
          .trailing = PIN(1, 25),
          .channel = 1,
          .dma = 0,
          .inversion = false
      }
  };

  struct Interface * const interface = init(MemoryBusDma, &busConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupDisplayBusSimple(void)
{
  static const PinNumber gpioBusPins[] = {
      PIN(2, 0), PIN(2, 1), PIN(2, 2), PIN(2, 3),
      PIN(2, 4), PIN(2, 5), PIN(2, 6), PIN(2, 7),
      0
  };
  static const struct FastGpioBusConfig gpioBusConfig = {
      .pins = gpioBusPins,
      .direction = PIN_OUTPUT
  };

  pinInput(pinInit(PIN(1, 19)));
  pinOutput(pinInit(PIN(1, 22)), true);
  pinOutput(pinInit(PIN(1, 25)), false);
  pinOutput(pinInit(PIN(1, 29)), false);

  struct GpioBus * const gpioBus = init(FastGpioBus, &gpioBusConfig);
  assert(gpioBus != NULL);

  const struct MemoryBusGpioConfig memoryBusConfig = {
      .bus = gpioBus,
      .cycle = 100,
      .frequency = 10000000,
      .inversion = false,
      .strobe = PIN(1, 28),
      .timer = 0
  };

  struct Interface * const interface = init(MemoryBusGpio, &memoryBusConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupI2C(void)
{
  static const struct I2CConfig i2cConfig = {
      .rate = 100000,
      .scl = PIN(0, 11),
      .sda = PIN(0, 10),
      .channel = 2
  };

  struct Interface * const interface = init(I2C, &i2cConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupIrda(bool master)
{
  const struct IrdaConfig irdaConfig = {
      .rate = 115200,
      .rxLength = BOARD_UART_BUFFER,
      .txLength = BOARD_UART_BUFFER,
      .frameLength = BOARD_UART_BUFFER / 4,
      .rx = PIN(0, 3),
      .tx = PIN(0, 2),
      .channel = 0,
      .timer = 0,
      .inversion = false,
      .master = master
  };

  struct Interface * const interface = init(Irda, &irdaConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct StreamPackage boardSetupI2S(void)
{
  static const struct I2SDmaConfig i2sConfig = {
      .size = 2,
      .rate = 96000,
      .width = I2S_WIDTH_16,
      .tx = {
          .sda = PIN(0, 9),
          .sck = PIN(0, 7),
          .ws = PIN(0, 8),
          .mclk = PIN(4, 29),
          .dma = 6
      },
      .rx = {
          .sda = PIN(0, 6),
          .dma = 7
      },
      .channel = 0,
      .mono = false,
      .slave = false
  };

  struct I2SDma * const interface = init(I2SDma, &i2sConfig);
  assert(interface != NULL);

  struct Stream * const rxStream = i2sDmaGetInput(interface);
  assert(rxStream != NULL);
  struct Stream * const txStream = i2sDmaGetOutput(interface);
  assert(txStream != NULL);

  return (struct StreamPackage){
      (struct Interface *)interface,
      rxStream,
      txStream
  };
}

/*----------------------------------------------------------------------------*/
struct Interface *boardSetupOneWire(void)
{
  static const struct OneWireSspConfig owConfig = {
      .miso = PIN(0, 8),
      .mosi = PIN(0, 9),
      .channel = 1
  };

  struct Interface * const interface = init(OneWireSsp, &owConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardSetupSensorEvent(enum InputEvent edge, enum PinPull pull)
{
  const struct PinIntConfig eventIntConfig = {
      .pin = BOARD_SENSOR_INT,
      .event = edge,
      .pull = pull
  };

  struct Interrupt * const interrupt = init(PinInt, &eventIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSerial(void)
{
  static const struct SerialConfig serialConfig = {
      .rxLength = BOARD_UART_BUFFER,
      .txLength = BOARD_UART_BUFFER,
      .rate = 19200,
      .rx = PIN(0, 16),
      .tx = PIN(0, 15),
      .channel = 1
  };

  struct Interface * const interface = init(Serial, &serialConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpi0(void)
{
  static const struct SpiDmaConfig spiDma0Config = {
      .rate = 2000000,
      .miso = PIN(0, 17),
      .mosi = PIN(0, 18),
      .sck = PIN(1, 20),
      .channel = 0,
      .mode = 0,
      .dma = {0, 1}
  };

  struct Interface * const interface = init(SpiDma, &spiDma0Config);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpi1(void)
{
  static const struct SpiDmaConfig spiDma1Config = {
      .rate = 2000000,
      .miso = PIN(0, 8),
      .mosi = PIN(0, 9),
      .sck = PIN(0, 7),
      .channel = 1,
      .mode = 0,
      .dma = {0, 1}
  };

  struct Interface * const interface = init(SpiDma, &spiDma1Config);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer0(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .event = GPTIMER_MATCH0,
      .channel = 0
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer1(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .event = GPTIMER_MATCH0,
      .channel = 1
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer2(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .event = GPTIMER_MATCH0,
      .channel = 2
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer3(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .event = GPTIMER_MATCH0,
      .channel = 3
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardSetupTouchEvent(enum InputEvent edge, enum PinPull pull)
{
  const struct PinIntConfig eventIntConfig = {
      .pin = BOARD_TOUCH_INT,
      .event = edge,
      .pull = pull
  };

  struct Interrupt * const interrupt = init(PinInt, &eventIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Entity *boardSetupUsb(void)
{
  /* Clocks */
  static const struct GenericClockConfig usbClockConfig = {
      .source = CLOCK_USB_PLL
  };
  static const struct PllConfig usbPllConfig = {
      .divisor = 4,
      .multiplier = 16,
      .source = CLOCK_EXTERNAL
  };

  /* Objects */
  static const struct UsbDeviceConfig usbConfig = {
      .dm = PIN(0, 30),
      .dp = PIN(0, 29),
      .connect = PIN(2, 9),
      .vbus = PIN(1, 30),
      .vid = 0x15A2,
      .pid = 0x0044,
      .channel = 0
  };

  clockEnable(UsbPll, &usbPllConfig);
  while (!clockReady(UsbPll));

  clockEnable(UsbClock, &usbClockConfig);
  while (!clockReady(UsbClock));

  struct Entity * const usb = init(UsbDevice, &usbConfig);
  assert(usb != NULL);
  return usb;
}
