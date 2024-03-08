/*
 * m48x_default/shared/board.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/platform/numicro/clocking.h>
#include <halm/platform/numicro/gptimer.h>
#include <halm/platform/numicro/hsusb_device.h>
#include <halm/platform/numicro/i2c.h>
#include <halm/platform/numicro/pin_int.h>
#include <halm/platform/numicro/serial.h>
#include <halm/platform/numicro/spi_dma.h>
#include <halm/platform/numicro/spim.h>
#include <halm/platform/numicro/qspi.h>
#include <halm/platform/numicro/usb_device.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
[[gnu::alias("boardSetupTimer0")]] struct Timer *boardSetupTimer(void);
[[gnu::alias("boardSetupTimer1")]] struct Timer *boardSetupTimerAux0(void);
[[gnu::alias("boardSetupTimer2")]] struct Timer *boardSetupTimerAux1(void);

[[gnu::alias("boardSetupUsbHs")]] struct Entity *boardSetupUsb(void);
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};

static const struct ExtendedClockConfig qspiClockConfig = {
    .divisor = 1,
    .source = CLOCK_APB
};

static const struct ExtendedClockConfig spiClockConfig = {
    .divisor = 1,
    .source = CLOCK_APB
};

static const struct ExtendedClockConfig uartClockConfig = {
    .divisor = 1,
    .source = CLOCK_PLL
};
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

  clockEnable(Apb0Clock, &apbClockConfigDirect);
  clockEnable(Apb1Clock, &apbClockConfigDirect);
  clockEnable(MainClock, &mainClockConfigExt);
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
struct Interrupt *boardSetupButton(void)
{
  static const struct PinIntConfig buttonIntConfig = {
      .pin = BOARD_BUTTON,
      .event = INPUT_FALLING,
      .pull = PIN_NOPULL
  };

  struct Interrupt * const interrupt = init(PinInt, &buttonIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupI2C(void)
{
  static const struct I2CConfig i2cConfig = {
      .rate = 100000,
      .scl = PIN(PORT_G, 0),
      .sda = PIN(PORT_G, 1),
      .channel = 0
  };

  struct Interface * const interface = init(I2C, &i2cConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupQspi(void)
{
  static const struct QspiConfig qspiConfig = {
      .rate = 1000000,
      .cs = 0,
      .io0 = PIN(PORT_C, 0),
      .io1 = PIN(PORT_C, 1),
      .io2 = PIN(PORT_C, 5),
      .io3 = PIN(PORT_C, 4),
      .sck = PIN(PORT_C, 2),
      .channel = 0,
      .mode = 0,
      .dma = {DMA0_CHANNEL0, DMA0_CHANNEL1}
  };

  clockEnable(qspiConfig.channel ? Qspi1Clock : Qspi0Clock, &qspiClockConfig);

  struct Interface * const interface = init(Qspi, &qspiConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSerial(void)
{
  static const struct SerialConfig serialConfig = {
      .rxLength = BOARD_UART_BUFFER,
      .txLength = BOARD_UART_BUFFER,
      .rate = 19200,
      .rx = PIN(PORT_A, 0),
      .tx = PIN(PORT_A, 1),
      .channel = 0
  };
  const void * const UART_CLOCKS[] = {
      Uart0Clock, Uart1Clock, Uart2Clock, Uart3Clock,
      Uart4Clock, Uart5Clock, Uart6Clock, Uart7Clock
  };

  clockEnable(UART_CLOCKS[serialConfig.channel], &uartClockConfig);

  struct Interface * const interface = init(Serial, &serialConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpi(void)
{
  static const struct SpiDmaConfig spiDmaConfig = {
      .rate = 2000000,
      .miso = PIN(PORT_C, 12),
      .mosi = PIN(PORT_C, 11),
      .sck = PIN(PORT_C, 10),
      .channel = 3,
      .mode = 0,
      .dma = {DMA0_CHANNEL0, DMA0_CHANNEL1}
  };

  clockEnable(Spi0Clock, &spiClockConfig);

  struct Interface * const interface = init(SpiDma, &spiDmaConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpim(struct Timer *timer)
{
  const struct SpimConfig spimConfig = {
      .timer = timer,
      .rate = 3000000,
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
struct Timer *boardSetupTimer0(void)
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
struct Timer *boardSetupTimer1(void)
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
struct Timer *boardSetupTimer2(void)
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
struct Timer *boardSetupTimer3(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .channel = 3
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
