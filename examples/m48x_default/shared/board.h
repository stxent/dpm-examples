/*
 * m48x_default/shared/board.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef M48X_DEFAULT_SHARED_BOARD_H_
#define M48X_DEFAULT_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/work_queue_irq.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON_0    PIN(PORT_G, 15)
#define BOARD_BUTTON_1    PIN(PORT_F, 11)
#define BOARD_BUTTON      BOARD_BUTTON_0
#define BOARD_LED_0       PIN(PORT_H, 0)
#define BOARD_LED_1       PIN(PORT_H, 1)
#define BOARD_LED_2       PIN(PORT_H, 2)
#define BOARD_LED         BOARD_LED_0
#define BOARD_LED_INV     true
#define BOARD_SPI_CS      PIN(PORT_C, 9)
#define BOARD_QSPI_CS     PIN(PORT_C, 3)
#define BOARD_UART_BUFFER 512

#define BOARD_USB_IND0    BOARD_LED_1
#define BOARD_USB_IND1    BOARD_LED_2
#define BOARD_USB_CDC_INT 0x81
#define BOARD_USB_CDC_RX  0x02
#define BOARD_USB_CDC_TX  0x82
#define BOARD_USB_MSC_RX  0x01
#define BOARD_USB_MSC_TX  0x81

DEFINE_WQ_IRQ(WQ_LP)
/*----------------------------------------------------------------------------*/
struct Entity;
struct Interface;
struct Interrupt;
struct Timer;
/*----------------------------------------------------------------------------*/
void boardSetupClockExt(void);
void boardSetupClockPll(void);
struct Interrupt *boardSetupButton(void);
struct Interface *boardSetupI2C(void);
struct Interface *boardSetupQspi(void);
struct Interface *boardSetupSerial(void);
struct Interface *boardSetupSpi(void);
struct Interface *boardSetupSpim(struct Timer *);
struct Timer *boardSetupTimer(void);
struct Timer *boardSetupTimer0(void);
struct Timer *boardSetupTimer1(void);
struct Timer *boardSetupTimer2(void);
struct Timer *boardSetupTimer3(void);
struct Timer *boardSetupTimerAux0(void);
struct Timer *boardSetupTimerAux1(void);
struct Entity *boardSetupUsb(void);
struct Entity *boardSetupUsbFs(void);
struct Entity *boardSetupUsbHs(void);
/*----------------------------------------------------------------------------*/
#endif /* M48X_DEFAULT_SHARED_BOARD_H_ */
