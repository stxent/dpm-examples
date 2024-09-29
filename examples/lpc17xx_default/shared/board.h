/*
 * lpc17xx_default/shared/board.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LPC17XX_DEFAULT_SHARED_BOARD_H_
#define LPC17XX_DEFAULT_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON            PIN(2, 10)
#define BOARD_BUTTON_INV        true
#define BOARD_LED_0             PIN(1, 10)
#define BOARD_LED_1             PIN(1, 9)
#define BOARD_LED_2             PIN(1, 8)
#define BOARD_LED               BOARD_LED_0
#define BOARD_LED_INV           false
#define BOARD_SPI0_CS0          PIN(0, 22)
/* Chip Select on the SPI connector */
#define BOARD_SPI1_CS0          PIN(0, 6)
/* Chip Select for the Touch Sensor */
#define BOARD_SPI1_CS1          PIN(1, 15)
#define BOARD_SPI_CS            BOARD_SPI1_CS0
#define BOARD_UART_BUFFER       512

#define BOARD_DISPLAY_BL        PIN(1, 26)
#define BOARD_DISPLAY_CS        PIN(1, 14)
#define BOARD_DISPLAY_RESET     PIN(1, 4)
#define BOARD_DISPLAY_RS        PIN(1, 0)
#define BOARD_DISPLAY_RW        PIN(1, 1)

#define BOARD_DISPLAY_SPI_BL    BOARD_DISPLAY_BL
#define BOARD_DISPLAY_SPI_CS    BOARD_SPI1_CS0
#define BOARD_DISPLAY_SPI_RESET PIN(4, 29)
#define BOARD_DISPLAY_SPI_RS    PIN(4, 28)

#define BOARD_TOUCH_CS          BOARD_SPI1_CS1
#define BOARD_TOUCH_INT         BOARD_BUTTON
#define BOARD_SENSOR_CS_0       BOARD_SPI1_CS0
#define BOARD_SENSOR_CS_1       PIN(4, 29)
#define BOARD_SENSOR_CS_2       PIN(4, 28)
#define BOARD_SENSOR_CS         BOARD_SENSOR_CS_0
#define BOARD_SENSOR_INT_0      PIN(0, 3)
#define BOARD_SENSOR_INT_1      PIN(0, 2)
#define BOARD_SENSOR_INT        BOARD_SENSOR_INT_0

#define BOARD_USB_IND0          BOARD_LED_1
#define BOARD_USB_IND1          BOARD_LED_2
#define BOARD_USB_CDC_INT       0x81
#define BOARD_USB_CDC_RX        0x02
#define BOARD_USB_CDC_TX        0x82
#define BOARD_USB_MSC_RX        0x02
#define BOARD_USB_MSC_TX        0x82

DEFINE_WQ_IRQ(WQ_LP)
/*----------------------------------------------------------------------------*/
struct Entity;
struct Interface;
struct Interrupt;
struct Stream;
struct Timer;

struct StreamPackage
{
  struct Interface *interface;
  struct Stream *rx;
  struct Stream *tx;
};
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void);
void boardSetupClockPll(void);
void boardSetupDefaultWQ(void);
void boardSetupLowPriorityWQ(void);
struct Interrupt *boardSetupButton(enum InputEvent);
struct Interface *boardSetupDisplayBus(void);
struct Interface *boardSetupDisplayBusDma(void);
struct Interface *boardSetupDisplayBusSimple(void);
struct Interface *boardSetupI2C(void);
struct StreamPackage boardSetupI2S(void);
struct Interface *boardSetupIrda(bool);
struct Interface *boardSetupOneWire(void);
struct Interrupt *boardSetupSensorEvent(enum InputEvent, enum PinPull);
struct Interrupt *boardSetupSensorEvent0(enum InputEvent, enum PinPull);
struct Interrupt *boardSetupSensorEvent1(enum InputEvent, enum PinPull);
struct Interface *boardSetupSerial(void);
struct Interface *boardSetupSerial1(void);
struct Interface *boardSetupSerial3(void);
struct Interface *boardSetupSerialAux(void);
struct Interface *boardSetupSpi(void);
struct Interface *boardSetupSpi0(void);
struct Interface *boardSetupSpi1(void);
struct Interface *boardSetupSpiDisplay(void);
struct Timer *boardSetupTimer(void);
struct Timer *boardSetupTimer0(void);
struct Timer *boardSetupTimer1(void);
struct Timer *boardSetupTimer2(void);
struct Timer *boardSetupTimer3(void);
struct Timer *boardSetupTimerAux0(void);
struct Timer *boardSetupTimerAux1(void);
struct Interrupt *boardSetupTouchEvent(enum InputEvent, enum PinPull);
struct Entity *boardSetupUsb(void);
/*----------------------------------------------------------------------------*/
#endif /* LPC17XX_DEFAULT_SHARED_BOARD_H_ */
