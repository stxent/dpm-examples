/*
 * m48x_dfu/shared/board.h
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef M48X_DFU_SHARED_BOARD_H_
#define M48X_DFU_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/flash.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON_0    PIN(PORT_G, 15)
#define BOARD_BUTTON_1    PIN(PORT_F, 11)
#define BOARD_BUTTON      BOARD_BUTTON_0
#define BOARD_LED_0       PIN(PORT_H, 0)
#define BOARD_LED_1       PIN(PORT_H, 1)
#define BOARD_LED_2       PIN(PORT_H, 2)
#define BOARD_LED         BOARD_LED_0
#define BOARD_LED_INV     true
/*----------------------------------------------------------------------------*/
struct Dfu;
struct DfuBridge;
struct Interface;
struct Interrupt;
struct Timer;
struct Usb;

struct ButtonPackage
{
  struct Interrupt *event;
  struct Timer *timer;
  struct Interrupt *button;
};

struct DfuPackage
{
  struct Timer *timer;
  struct Usb *usb;
  struct Dfu *dfu;
  struct DfuBridge *bridge;
};

struct MemoryPackage
{
  struct Timer *timer;
  struct Interface *spim;
  struct Interface *flash;

  struct FlashGeometry geometry[2];
  size_t offset;
  size_t regions;
};
/*----------------------------------------------------------------------------*/
void boardClockPostUpdate(struct Interface *);
void boardResetClock(void);
void boardResetClockPartial(void);
void boardSetupClockExt(void);
void boardSetupClockInt(void);
void boardSetupClockPll(void);
void boardSetupDefaultWQ(void);
void boardSetupMemoryFlash(struct MemoryPackage *);
void boardSetupMemoryNOR(struct MemoryPackage *);
struct Interface *boardSetupSerial(void);
struct Interface *boardSetupSpim(struct Timer *);
struct Timer *boardSetupTimer(void);
struct Timer *boardSetupTimerAux0(void);
struct Timer *boardSetupTimerAux1(void);
struct Timer *boardSetupTimerAux2(void);
struct Usb *boardSetupUsb(void);
struct Usb *boardSetupUsbFs(void);
struct Usb *boardSetupUsbHs(void);

void boardSetupButtonPackage(struct ButtonPackage *);
void boardSetupDfuPackage(struct DfuPackage *, struct Interface *,
    struct FlashGeometry *, size_t, size_t, void (*)(void));
/*----------------------------------------------------------------------------*/
#endif /* M48X_DFU_SHARED_BOARD_H_ */
