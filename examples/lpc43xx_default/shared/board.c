/*
 * lpc43xx_default/shared/board.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include <dpm/platform/lpc/sgpio_bus.h>
#include <halm/delay.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/i2c.h>
#include <halm/platform/lpc/i2s_dma.h>
#include <halm/platform/lpc/pin_int.h>
#include <halm/platform/lpc/serial.h>
#include <halm/platform/lpc/spi_dma.h>
#include <halm/platform/lpc/spifi.h>
#include <halm/platform/lpc/system.h>
#include <halm/platform/lpc/usb_device.h>
#include <assert.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
[[gnu::alias("boardSetupI2C1")]] struct Interface *boardSetupI2C(void);

[[gnu::alias("boardSetupSensorEvent0")]]
    struct Interrupt *boardSetupSensorEvent(enum InputEvent, enum PinPull);

[[gnu::alias("boardSetupSerial1")]] struct Interface *boardSetupSerial(void);
[[gnu::alias("boardSetupSerial2")]] struct Interface *boardSetupSerialAux(void);

[[gnu::alias("boardSetupSpi0")]] struct Interface *boardSetupSpi(void);
[[gnu::alias("boardSetupSpi0")]] struct Interface *boardSetupSpiDisplay(void);

[[gnu::alias("boardSetupTimer3")]] struct Timer *boardSetupTimer(void);
[[gnu::alias("boardSetupTimer1")]] struct Timer *boardSetupTimerAux0(void);
[[gnu::alias("boardSetupTimer2")]] struct Timer *boardSetupTimerAux1(void);

[[gnu::alias("boardSetupUsb0")]] struct Entity *boardSetupUsb(void);

static void enablePeriphClock(const void *);
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};

[[gnu::section(".shared")]] static struct ClockSettings sharedClockSettings;
/*----------------------------------------------------------------------------*/
DECLARE_WQ_IRQ(WQ_LP, SPI_ISR)
/*----------------------------------------------------------------------------*/
static void enablePeriphClock(const void *clock)
{
  if (clockReady(clock))
    clockDisable(clock);

  if (clockReady(SystemPll))
    clockEnable(clock, &(struct GenericClockConfig){CLOCK_PLL});
  else if (clockReady(ExternalOsc))
    clockEnable(clock, &(struct GenericClockConfig){CLOCK_EXTERNAL});
  else
    clockEnable(clock, &(struct GenericClockConfig){CLOCK_INTERNAL});

  while (!clockReady(clock));
}
/*----------------------------------------------------------------------------*/
void boardResetClock(void)
{
  clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_INTERNAL});

  if (clockReady(AudioPll))
    clockDisable(AudioPll);

  if (clockReady(UsbPll))
    clockDisable(UsbPll);

  if (clockReady(SystemPll))
    clockDisable(SystemPll);

  if (clockReady(ExternalOsc))
    clockDisable(ExternalOsc);

  /* Flash latency should be reset to exit correctly from power-down modes */
  sysFlashLatencyReset();
}
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void)
{
  bool clockSettingsLoaded = loadClockSettings(&sharedClockSettings);
  const bool spifiClockEnabled = clockReady(SpifiClock);

  if (clockSettingsLoaded)
  {
    /* Check clock sources */
    if (!clockReady(ExternalOsc) || !clockReady(SystemPll))
    {
      memset(&sharedClockSettings, 0, sizeof(sharedClockSettings));
      clockSettingsLoaded = false;
    }
  }

  if (!clockSettingsLoaded)
  {
    clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_INTERNAL});

    if (spifiClockEnabled)
    {
      /* Running from NOR Flash, switch SPIFI clock to IRC without disabling */
      clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_INTERNAL});
    }

    clockEnable(ExternalOsc, &extOscConfig);
    while (!clockReady(ExternalOsc));

    clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_EXTERNAL});

    if (spifiClockEnabled)
    {
      /* Switch SPIFI clock to external crystal */
      clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_EXTERNAL});
    }

    /* Disable unused System PLL */
    if (clockReady(SystemPll))
      clockDisable(SystemPll);
  }
}
/*----------------------------------------------------------------------------*/
void boardSetupClockPll(void)
{
  static const struct GenericDividerConfig sysDivConfig = {
      .divisor = 2,
      .source = CLOCK_PLL
  };
  static const struct PllConfig sysPllConfig = {
      .divisor = 1,
      .multiplier = 17,
      .source = CLOCK_EXTERNAL
  };

  bool clockSettingsLoaded = loadClockSettings(&sharedClockSettings);
  const bool spifiClockEnabled = clockReady(SpifiClock);

  if (clockSettingsLoaded)
  {
    /* Check clock sources */
    if (!clockReady(ExternalOsc) || !clockReady(SystemPll))
    {
      memset(&sharedClockSettings, 0, sizeof(sharedClockSettings));
      clockSettingsLoaded = false;
    }
  }

  if (!clockSettingsLoaded)
  {
    clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_INTERNAL});

    if (spifiClockEnabled)
    {
      /* Running from NOR Flash, switch SPIFI clock to IRC without disabling */
      clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_INTERNAL});
    }

    clockEnable(ExternalOsc, &extOscConfig);
    while (!clockReady(ExternalOsc));

    clockEnable(SystemPll, &sysPllConfig);
    while (!clockReady(SystemPll));

    if (sysPllConfig.divisor == 1)
    {
      /* High frequency, make a PLL clock divided by 2 for base clock ramp up */
      clockEnable(DividerA, &sysDivConfig);
      while (!clockReady(DividerA));

      clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_IDIVA});
      udelay(50);
      clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_PLL});

      /* Base clock is ready, temporary clock divider is not needed anymore */
      clockDisable(DividerA);
    }
    else
    {
      /* Low CPU frequency */
      clockEnable(MainClock, &(struct GenericClockConfig){CLOCK_PLL});
    }
  }

  /* SPIFI */
  if (!clockSettingsLoaded && spifiClockEnabled)
  {
    static const uint32_t spifiMaxFrequency = 30000000;
    const uint32_t frequency = clockFrequency(SystemPll);

    /* Running from NOR Flash, update SPIFI clock without disabling */
    if (frequency > spifiMaxFrequency)
    {
      const struct GenericDividerConfig spifiDivConfig = {
          .divisor = (frequency + spifiMaxFrequency - 1) / spifiMaxFrequency,
          .source = CLOCK_PLL
      };

      clockEnable(DividerD, &spifiDivConfig);
      while (!clockReady(DividerD));

      clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_IDIVD});
    }
    else
    {
      clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_PLL});
    }
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
      .irq = SPI_IRQ,
      .priority = 0
  };

  WQ_LP = init(WorkQueueIrq, &wqIrqConfig);
  assert(WQ_LP != NULL);
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardSetupButton(enum InputEvent event)
{
  const struct PinIntConfig buttonIntConfig = {
      .pin = BOARD_BUTTON,
      .event = event,
      .pull = BOARD_BUTTON_INV ? PIN_PULLUP : PIN_PULLDOWN
  };

  struct Interrupt * const interrupt = init(PinInt, &buttonIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupDisplayBus(void)
{
  static const struct SgpioBusConfig sgpioBusConfig = {
      .prescaler = 4,
      .dma = 0,
      .priority = 0,
      .inversion = false,

      .pins = {
          .clock = PIN(PORT_6, 3), /* SGPIO_4 */
          .data = {
              PIN(PORT_4, 2), /* SGPIO_8 */
              PIN(PORT_4, 3), /* SGPIO_9 */
              PIN(PORT_4, 4), /* SGPIO_10 */
              PIN(PORT_4, 5), /* SGPIO_11 */
              PIN(PORT_4, 6), /* SGPIO_12 */
              PIN(PORT_4, 8), /* SGPIO_13 */
              PIN(PORT_4, 9), /* SGPIO_14 */
              PIN(PORT_4, 10) /* SGPIO_15 */
          },
          .dma = SGPIO_3
      },

      .slices = {
          .gate = SGPIO_SLICE_P,
          .qualifier = SGPIO_SLICE_A /* SGPIO_0 */
      }
  };

  /* Clock for SGPIO register interface */
  enablePeriphClock(PeriphClock);

  /* Timer 0 will be used as a DMA event source */
  struct Interface * const interface = init(SgpioBus, &sgpioBusConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupI2C0(void)
{
  static const struct I2CConfig i2c0Config = {
      .rate = 100000,
      .scl = PIN(PORT_I2C, PIN_I2C0_SCL),
      .sda = PIN(PORT_I2C, PIN_I2C0_SDA),
      .channel = 0
  };

  /* I2C0 is connected to the APB1 bus */
  enablePeriphClock(Apb1Clock);

  struct Interface * const interface = init(I2C, &i2c0Config);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupI2C1(void)
{
  static const struct I2CConfig i2c1Config = {
      .rate = 100000,
      .scl = PIN(PORT_2, 4),
      .sda = PIN(PORT_2, 3),
      .channel = 1
  };

  /* I2C1 is connected to the APB3 bus */
  enablePeriphClock(Apb3Clock);

  struct Interface * const interface = init(I2C, &i2c1Config);
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
          .sda = PIN(PORT_7, 2),
          .sck = PIN(PORT_4, 7),
          .ws = PIN(PORT_7, 1),
          .mclk = PIN(PORT_CLK, 2),
          .dma = 6
      },
      .rx = {
          .sda = PIN(PORT_6, 2),
          .dma = 7
      },
      .channel = 0,
      .mono = false,
      .slave = false
  };

  /* I2S are connected to the APB1 bus */
  enablePeriphClock(Apb1Clock);

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
struct Interrupt *boardSetupSensorEvent0(enum InputEvent edge,
    enum PinPull pull)
{
  const struct PinIntConfig eventIntConfig = {
      .pin = BOARD_SENSOR_INT_0,
      .priority = 0,
      .event = edge,
      .pull = pull
  };

  struct Interrupt * const interrupt = init(PinInt, &eventIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardSetupSensorEvent1(enum InputEvent edge,
    enum PinPull pull)
{
  const struct PinIntConfig eventIntConfig = {
      .pin = BOARD_SENSOR_INT_1,
      .priority = 0,
      .event = edge,
      .pull = pull
  };

  struct Interrupt * const interrupt = init(PinInt, &eventIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSerial1(void)
{
  static const struct SerialConfig serialConfig = {
      .rxLength = BOARD_UART_BUFFER,
      .txLength = BOARD_UART_BUFFER,
      .rate = 19200,
      .rx = PIN(PORT_1, 14),
      .tx = PIN(PORT_5, 6),
      .channel = 1
  };

  enablePeriphClock(Uart1Clock);

  struct Interface * const interface = init(Serial, &serialConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSerial2(void)
{
  static const struct SerialConfig serialConfig = {
      .rxLength = BOARD_UART_BUFFER,
      .txLength = BOARD_UART_BUFFER,
      .rate = 19200,
      .rx = PIN(PORT_2, 11),
      .tx = PIN(PORT_2, 10),
      .channel = 2
  };

  enablePeriphClock(Usart2Clock);

  struct Interface * const interface = init(Serial, &serialConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpi0(void)
{
  static const struct SpiDmaConfig spiDma0Config = {
      .rate = 2000000,
      .sck = PIN(PORT_3, 0),
      .miso = PIN(PORT_1, 1),
      .mosi = PIN(PORT_1, 2),
      .channel = 0,
      .mode = 0,
      .dma = {0, 1}
  };

  enablePeriphClock(Ssp0Clock);

  struct Interface * const interface = init(SpiDma, &spiDma0Config);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpi1(void)
{
  static const struct SpiDmaConfig spiDma1Config = {
      .rate = 2000000,
      .sck = PIN(PORT_F, 4),
      .miso = PIN(PORT_1, 3),
      .mosi = PIN(PORT_1, 4),
      .channel = 1,
      .mode = 0,
      .dma = {0, 1}
  };

  enablePeriphClock(Ssp1Clock);

  struct Interface * const interface = init(SpiDma, &spiDma1Config);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardSetupSpim(struct Timer *)
{
  /* Objects */
  static const struct SpifiConfig spifiConfig = {
      .delay = 1,
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

  if (clockReady(SystemPll))
  {
    /* Use safe settings, maximum possible frequency for SPIFI is 104 MHz */
    static const uint32_t spifiMaxFrequency = 30000000;
    const uint32_t frequency = clockFrequency(SystemPll);

    if (frequency > spifiMaxFrequency)
    {
      const struct GenericDividerConfig spifiDivConfig = {
          .divisor = (frequency + spifiMaxFrequency - 1) / spifiMaxFrequency,
          .source = CLOCK_PLL
      };

      clockEnable(DividerD, &spifiDivConfig);
      while (!clockReady(DividerD));

      clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_IDIVD});
    }
    else
    {
      clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_PLL});
    }
  }
  else if (clockReady(ExternalOsc))
  {
    clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_EXTERNAL});
    while (!clockReady(SpifiClock));
  }
  else
  {
    clockEnable(SpifiClock, &(struct GenericClockConfig){CLOCK_INTERNAL});
    while (!clockReady(SpifiClock));
  }

  struct Interface * const interface = init(Spifi, &spifiConfig);
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
      .priority = 0,
      .event = edge,
      .pull = pull
  };

  struct Interrupt * const interrupt = init(PinInt, &eventIntConfig);
  assert(interrupt != NULL);
  return interrupt;
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
      .divisor = 4,
      .multiplier = 40,
      .source = CLOCK_EXTERNAL
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
