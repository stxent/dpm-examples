/*
 * stm32f4xx_default/shared/board.h
 * Copyright (C) 2026 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef STM32F4XX_DEFAULT_SHARED_BOARD_H_
#define STM32F4XX_DEFAULT_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON        PIN(PORT_A, 0)
#define BOARD_BUTTON_INV    false
#define BOARD_LED_0         PIN(PORT_C, 13)
#define BOARD_LED_1         PIN(PORT_B, 2)
#define BOARD_LED_2         PIN(PORT_A, 15)
#define BOARD_LED           BOARD_LED_0
#define BOARD_LED_INV       false
#define BOARD_SPI_CS_0      PIN(PORT_A, 3)
#define BOARD_SPI_CS_1      PIN(PORT_B, 12)
#define BOARD_SPI_CS_2      PIN(PORT_A, 4)
#define BOARD_UART_BUFFER   512

#define BOARD_MEM_CS        BOARD_SPI_CS_2
#define BOARD_SPI_CS        BOARD_SPI_CS_0

#define BOARD_SENSOR_CS_0   BOARD_SPI_CS
#define BOARD_SENSOR_CS     BOARD_SENSOR_CS_0
#define BOARD_SENSOR_INT_0  PIN(PORT_A, 8)
#define BOARD_SENSOR_INT_1  PIN(PORT_A, 10)
#define BOARD_SENSOR_INT    BOARD_SENSOR_INT_0

#define BOARD_USB_CDC_INT   0x81
#define BOARD_USB_CDC_RX    0x02
#define BOARD_USB_CDC_TX    0x82

DEFINE_WQ_IRQ(WQ_LP)
/*----------------------------------------------------------------------------*/
struct Interface;
struct Interrupt;
struct Timer;
struct Usb;
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void);
void boardSetupClockPll(void);
void boardSetupDefaultWQ(void);
void boardSetupLowPriorityWQ(void);
struct Interrupt *boardSetupButton(void);
struct Interface *boardSetupFlash(void);
struct Interface *boardSetupI2C(void);
struct Interface *boardSetupI2C1(void);
struct Interface *boardSetupI2C2(void);
struct Interrupt *boardSetupSensorEvent(enum InputEvent, enum PinPull);
struct Interface *boardSetupSerial(void);
struct Interface *boardSetupSerial1(void);
struct Interface *boardSetupSerial2(void);
struct Interface *boardSetupSerialAux(void);
struct Interface *boardSetupSpi(void);
struct Interface *boardSetupSpi1(void);
struct Interface *boardSetupSpi2(void);
struct Timer *boardSetupTimer(void);
struct Timer *boardSetupTimer5(void);
struct Timer *boardSetupTimer6(void);
struct Timer *boardSetupTimer7(void);
struct Timer *boardSetupTimerAux(void);
struct Timer *boardSetupTimerAux0(void);
struct Timer *boardSetupTimerAux1(void);
struct Usb *boardSetupUsb(void);
struct Interface *boardSetupUsbSerial(void);
/*----------------------------------------------------------------------------*/
#endif /* STM32F4XX_DEFAULT_SHARED_BOARD_H_ */
