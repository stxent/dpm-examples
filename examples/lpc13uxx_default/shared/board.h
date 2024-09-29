/*
 * lpc13uxx_default/shared/board.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LPC13UXX_DEFAULT_SHARED_BOARD_H_
#define LPC13UXX_DEFAULT_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON      PIN(0, 3)
#define BOARD_BUTTON_INV  true
#define BOARD_CAPTURE     PIN(0, 17)
#define BOARD_LED_0       PIN(1, 22)
#define BOARD_LED_1       PIN(1, 14)
#define BOARD_LED_2       PIN(1, 13)
#define BOARD_LED         BOARD_LED_0
#define BOARD_LED_INV     false
#define BOARD_PWM_0       PIN(0, 22)
#define BOARD_PWM_1       PIN(0, 21)
#define BOARD_PWM         BOARD_PWM_0
#define BOARD_SPI_CS      PIN(0, 2)
#define BOARD_UART_BUFFER 256

#define BOARD_USB_IND0    BOARD_LED_1
#define BOARD_USB_IND1    BOARD_LED_2
#define BOARD_USB_CDC_INT 0x81
#define BOARD_USB_CDC_RX  0x02
#define BOARD_USB_CDC_TX  0x82

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
struct Interface *boardSetupIrda(bool);
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
/*----------------------------------------------------------------------------*/
#endif /* LPC13UXX_DEFAULT_SHARED_BOARD_H_ */
