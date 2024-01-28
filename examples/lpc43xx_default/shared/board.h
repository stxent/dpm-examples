/*
 * lpc43xx_default/shared/board.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LPC43XX_DEFAULT_SHARED_BOARD_H_
#define LPC43XX_DEFAULT_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON            PIN(PORT_2, 7)
#define BOARD_BUTTON_INV        true
#define BOARD_LED_0             PIN(PORT_5, 7)
#define BOARD_LED_1             PIN(PORT_5, 5)
#define BOARD_LED_2             PIN(PORT_4, 0)
#define BOARD_LED               BOARD_LED_0
#define BOARD_LED_INV           false
#define BOARD_PHY_RESET         PIN(PORT_5, 2)
/* Chip Select on the SPI connector */
#define BOARD_SPI0_CS0          PIN(PORT_1, 0)
/* Chip Select for the Touch Sensor */
#define BOARD_SPI0_CS1          PIN(PORT_5, 0)
#define BOARD_SPI1_CS0          PIN(PORT_1, 5)
#define BOARD_SPI_CS            PIN(PORT_5, 0)
#define BOARD_USB0_IND0         PIN(PORT_6, 8)
#define BOARD_USB0_IND1         PIN(PORT_6, 7)
#define BOARD_UART_BUFFER       512

#define BOARD_DISPLAY_BL        PIN(PORT_2, 12)
#define BOARD_DISPLAY_CS        PIN(PORT_2, 2)
#define BOARD_DISPLAY_RESET     PIN(PORT_6, 1)
#define BOARD_DISPLAY_RS        PIN(PORT_6, 6)
#define BOARD_DISPLAY_RW        PIN(PORT_2, 1)

#define BOARD_DISPLAY_SPI_BL    BOARD_DISPLAY_BL
#define BOARD_DISPLAY_SPI_CS    BOARD_SPI0_CS0
#define BOARD_DISPLAY_SPI_RESET PIN(PORT_4, 1)
#define BOARD_DISPLAY_SPI_RS    PIN(PORT_7, 7)

#define BOARD_TOUCH_CS          BOARD_SPI0_CS1
#define BOARD_TOUCH_INT         PIN(PORT_5, 1)
#define BOARD_SENSOR_CS_0       BOARD_SPI0_CS0
#define BOARD_SENSOR_CS_1       PIN(PORT_7, 6)
#define BOARD_SENSOR_CS_2       PIN(PORT_7, 5)
#define BOARD_SENSOR_CS         BOARD_SENSOR_CS_0
#define BOARD_SENSOR_INT_0      PIN(PORT_4, 1)
#define BOARD_SENSOR_INT_1      PIN(PORT_7, 7)

#define BOARD_USB_IND0          BOARD_USB0_IND0
#define BOARD_USB_IND1          BOARD_USB0_IND1
#define BOARD_USB_CDC_INT       0x81
#define BOARD_USB_CDC_RX        0x02
#define BOARD_USB_CDC_TX        0x83
#define BOARD_USB_MSC_RX        0x01
#define BOARD_USB_MSC_TX        0x81

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
struct Interrupt *boardSetupButton(void);
struct Interface *boardSetupDisplayBus(void);
struct Interface *boardSetupI2C(void);
struct Interface *boardSetupI2C0(void);
struct Interface *boardSetupI2C1(void);
struct StreamPackage boardSetupI2S(void);
struct Interrupt *boardSetupSensorEvent(enum InputEvent, enum PinPull);
struct Interrupt *boardSetupSensorEvent0(enum InputEvent, enum PinPull);
struct Interrupt *boardSetupSensorEvent1(enum InputEvent, enum PinPull);
struct Interface *boardSetupSerial(void);
struct Interface *boardSetupSpi(void);
struct Interface *boardSetupSpi0(void);
struct Interface *boardSetupSpi1(void);
struct Interface *boardSetupSpiDisplay(void);
struct Interface *boardSetupSpim(struct Timer *);
struct Timer *boardSetupTimer(void);
struct Timer *boardSetupTimer0(void);
struct Timer *boardSetupTimer1(void);
struct Timer *boardSetupTimer2(void);
struct Timer *boardSetupTimer3(void);
struct Timer *boardSetupTimerAux0(void);
struct Timer *boardSetupTimerAux1(void);
struct Interrupt *boardSetupTouchEvent(enum InputEvent, enum PinPull);
struct Entity *boardSetupUsb(void);
struct Entity *boardSetupUsb0(void);
struct Entity *boardSetupUsb1(void);
/*----------------------------------------------------------------------------*/
#endif /* LPC43XX_DEFAULT_SHARED_BOARD_H_ */
