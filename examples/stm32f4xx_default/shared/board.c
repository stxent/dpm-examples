/*
 * stm32f4xx_default/shared/board.c
 * Copyright (C) 2026 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <halm/generic/work_queue.h>
#include <halm/platform/stm32/clocking.h>
#include <halm/platform/stm32/exti.h>
#include <halm/platform/stm32/flash.h>
#include <halm/platform/stm32/gptimer.h>
#include <halm/platform/stm32/i2c.h>
#include <halm/platform/stm32/serial_dma.h>
#include <halm/platform/stm32/spi.h>
#include <halm/platform/stm32/usb_device.h>
#include <halm/usb/cdc_acm.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
[[gnu::alias("boardSetupI2C1")]] struct Interface *boardSetupI2C(void);

[[gnu::alias("boardSetupUsbSerial")]] struct Interface *boardSetupSerial(void);
[[gnu::alias("boardSetupSerial1")]] struct Interface *boardSetupSerialAux(void);

[[gnu::alias("boardSetupSpi1")]] struct Interface *boardSetupSpi(void);

[[gnu::alias("boardSetupTimer5")]] struct Timer *boardSetupTimer(void);
[[gnu::alias("boardSetupTimer6")]] struct Timer *boardSetupTimerAux(void);
[[gnu::alias("boardSetupTimer6")]] struct Timer *boardSetupTimerAux0(void);
[[gnu::alias("boardSetupTimer7")]] struct Timer *boardSetupTimerAux1(void);
/*----------------------------------------------------------------------------*/
enum
{
  PLL_CONFIG_8MHZ,
  PLL_CONFIG_12MHZ,
  PLL_CONFIG_16MHZ,
  PLL_CONFIG_25MHZ
};

static const struct ExternalOscConfig extOscConfig = {
    .frequency = 8000000
};

static const struct MainClockConfig mainClockConfig = {
    .divisor = 1,
    .range = VR_2V7_3V6
};

static const struct PllConfig pllConfigArray[] = {
    [PLL_CONFIG_8MHZ] = {
        .divisor = 2,
        .multiplier = 25,
        .source = CLOCK_EXTERNAL
    },
    [PLL_CONFIG_12MHZ] = {
        .divisor = 2,
        .multiplier = 16,
        .source = CLOCK_EXTERNAL
    },
    [PLL_CONFIG_16MHZ] = {
        .divisor = 2,
        .multiplier = 12,
        .source = CLOCK_EXTERNAL
    },
    [PLL_CONFIG_25MHZ] = {
        .divisor = 2,
        .multiplier = 8,
        .source = CLOCK_EXTERNAL
    }
};
/*----------------------------------------------------------------------------*/
DECLARE_WQ_IRQ(WQ_LP, FLASH_ISR)
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void)
{
  clockEnable(ExternalOsc, &extOscConfig);
  while (!clockReady(ExternalOsc));

  clockEnable(Apb1Clock, &(struct BusClockConfig){1});
  clockEnable(Apb2Clock, &(struct BusClockConfig){1});
  clockEnable(SystemClock, &(struct SystemClockConfig){CLOCK_EXTERNAL});

  clockEnable(MainClock, &mainClockConfig);
}
/*----------------------------------------------------------------------------*/
void boardSetupClockPll(void)
{
  const struct PllConfig *mainPllConfig = NULL;

  if (extOscConfig.frequency == 8000000)
    mainPllConfig = &pllConfigArray[PLL_CONFIG_8MHZ];
  else if (extOscConfig.frequency == 12000000)
    mainPllConfig = &pllConfigArray[PLL_CONFIG_12MHZ];
  else if (extOscConfig.frequency == 16000000)
    mainPllConfig = &pllConfigArray[PLL_CONFIG_16MHZ];
  else if (extOscConfig.frequency == 24000000)
    mainPllConfig = &pllConfigArray[PLL_CONFIG_25MHZ];
  assert(mainPllConfig != NULL);

  if (mainPllConfig != NULL)
  {
    clockEnable(ExternalOsc, &extOscConfig);
    while (!clockReady(ExternalOsc));

    clockEnable(MainPll, mainPllConfig);
    while (!clockReady(MainPll));

    clockEnable(Apb1Clock, &(struct BusClockConfig){4});
    clockEnable(Apb2Clock, &(struct BusClockConfig){2});
    clockEnable(SystemClock, &(struct SystemClockConfig){CLOCK_PLL});

    clockEnable(MainClock, &mainClockConfig);
  }
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
      .irq = FLASH_IRQ,
      .priority = 0
  };

  WQ_LP = init(WorkQueueIrq, &wqIrqConfig);
  assert(WQ_LP != NULL);
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardSetupButton(void)
{
  static const struct ExtiConfig buttonIntConfig = {
      .pin = BOARD_BUTTON,
      .event = BOARD_BUTTON_INV ? INPUT_FALLING : INPUT_RISING,
      .pull = BOARD_BUTTON_INV ? PIN_PULLUP : PIN_PULLDOWN
  };

  struct Interrupt * const interrupt = init(Exti, &buttonIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupFlash(void)
{
  static const struct FlashConfig flashConfig = {
      .voltage = VR_2V7_3V6
  };

  struct Interface * const interface = init(Flash, &flashConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupI2C1(void)
{
  static const struct I2CConfig i2cConfig = {
      .rate = 100000,
      .scl = PIN(PORT_B, 8),
      .sda = PIN(PORT_B, 9),
      .channel = I2C1,
      .rxDma = DMA1_STREAM0,
      .txDma = DMA1_STREAM7
  };

  struct Interface * const interface = init(I2C, &i2cConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupI2C2(void)
{
  static const struct I2CConfig i2cConfig = {
      .rate = 100000,
      .scl = PIN(PORT_B, 10),
      .sda = PIN(PORT_B, 3),
      .channel = I2C2,
      .rxDma = DMA1_STREAM2,
      .txDma = DMA1_STREAM7
  };

  struct Interface * const interface = init(I2C, &i2cConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardSetupSensorEvent(enum InputEvent edge, enum PinPull pull)
{
  const struct ExtiConfig eventIntConfig = {
      .pin = BOARD_SENSOR_INT,
      .event = edge,
      .pull = pull
  };

  struct Interrupt * const interrupt = init(Exti, &eventIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSerial1(void)
{
  static const struct SerialDmaConfig serialDmaConfig = {
      .rxChunk = BOARD_UART_BUFFER / 4,
      .rxLength = BOARD_UART_BUFFER,
      .txLength = BOARD_UART_BUFFER,
      .rate = 19200,
      .rx = PIN(PORT_B, 7),
      .tx = PIN(PORT_B, 6),
      .channel = USART1,
      .rxDma = DMA2_STREAM2,
      .txDma = DMA2_STREAM7
  };

  struct Interface * const interface = init(SerialDma, &serialDmaConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSerial2(void)
{
  static const struct SerialDmaConfig serialDmaConfig = {
      .rxChunk = BOARD_UART_BUFFER / 4,
      .rxLength = BOARD_UART_BUFFER,
      .txLength = BOARD_UART_BUFFER,
      .rate = 19200,
      .rx = PIN(PORT_A, 3),
      .tx = PIN(PORT_A, 2),
      .channel = USART2,
      .rxDma = DMA1_STREAM5,
      .txDma = DMA1_STREAM6
  };

  struct Interface * const interface = init(SerialDma, &serialDmaConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpi1(void)
{
  static const struct SpiConfig spiConfig = {
      .rate = 2000000,
      .miso = PIN(PORT_B, 4),
      .mosi = PIN(PORT_A, 7),
      .sck = PIN(PORT_A, 5),
      .channel = SPI1,
      .mode = 0,
      .rxDma = DMA2_STREAM2,
      .txDma = DMA2_STREAM3
  };

  struct Interface * const interface = init(Spi, &spiConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpi2(void)
{
  static const struct SpiConfig spiConfig = {
      .rate = 2000000,
      .miso = PIN(PORT_B, 14),
      .mosi = PIN(PORT_B, 15),
      .sck = PIN(PORT_B, 13),
      .channel = SPI2,
      .mode = 0,
      .rxDma = DMA1_STREAM3,
      .txDma = DMA1_STREAM4
  };

  struct Interface * const interface = init(Spi, &spiConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer5(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .channel = TIM5
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardSetupTimer6(void)
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
struct Timer *boardSetupTimer7(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 10000,
      .channel = TIM7
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
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupUsbSerial(void)
{
  struct Usb * const usb = boardSetupUsb();

  const struct CdcAcmConfig config = {
      .device = usb,
      .arena = NULL,
      .rxBuffers = 4,
      .txBuffers = 4,

      .endpoints = {
          .interrupt = BOARD_USB_CDC_INT,
          .rx = BOARD_USB_CDC_RX,
          .tx = BOARD_USB_CDC_TX
      }
  };

  struct Interface * const serial = init(CdcAcm, &config);
  assert(serial != NULL);

  usbDevSetConnected(usb, true);
  return serial;
}
