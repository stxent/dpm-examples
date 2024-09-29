/*
 * lpc13xx_default/shared/board.h
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LPC13XX_DEFAULT_SHARED_BOARD_H_
#define LPC13XX_DEFAULT_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON      PIN(0, 3)
#define BOARD_BUTTON_INV  true
#define BOARD_LED_0       PIN(2, 3)
#define BOARD_LED_1       PIN(3, 1)
#define BOARD_LED_2       PIN(3, 0)
#define BOARD_LED         BOARD_LED_0
#define BOARD_LED_INV     false
#define BOARD_SPI_CS      PIN(0, 2)
#define BOARD_UART_BUFFER 128

#define BOARD_USB_IND0    BOARD_LED_1
#define BOARD_USB_IND1    BOARD_LED_2
#define BOARD_USB_CDC_INT 0x81
#define BOARD_USB_CDC_RX  0x03
#define BOARD_USB_CDC_TX  0x83

DEFINE_WQ_IRQ(WQ_LP)
/*----------------------------------------------------------------------------*/
struct Entity;
struct Interface;
struct Interrupt;
struct Timer;
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void);
void boardSetupClockPll(void);
void boardSetupDefaultWQ(void);
void boardSetupLowPriorityWQ(void);
struct Interrupt *boardSetupButton(void);
struct Interface *boardSetupI2C(void);
struct Interface *boardSetupSerial(void);
struct Interface *boardSetupSpi(void);
struct Timer *boardSetupTimer(void);
struct Timer *boardSetupTimer16B0(void);
struct Timer *boardSetupTimer16B1(void);
struct Timer *boardSetupTimer32B0(void);
struct Timer *boardSetupTimer32B1(void);
struct Timer *boardSetupTimerAux0(void);
struct Timer *boardSetupTimerAux1(void);
struct Entity *boardSetupUsb(void);
struct Interface *boardSetupWS281x(size_t);
/*----------------------------------------------------------------------------*/
#endif /* LPC13XX_DEFAULT_SHARED_BOARD_H_ */
