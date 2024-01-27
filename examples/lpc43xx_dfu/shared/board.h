/*
 * lpc43xx_dfu/shared/board.h
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef LPC43XX_DFU_SHARED_BOARD_H_
#define LPC43XX_DFU_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/flash.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON    PIN(PORT_2, 7)
#define BOARD_LED_0     PIN(PORT_5, 7)
#define BOARD_LED_1     PIN(PORT_5, 5)
#define BOARD_LED_2     PIN(PORT_4, 0)
#define BOARD_LED       BOARD_LED_0
#define BOARD_LED_INV   false
#define BOARD_USB0_IND0 PIN(PORT_6, 8)
#define BOARD_USB0_IND1 PIN(PORT_6, 7)

#define BOARD_USB_IND0  BOARD_USB0_IND0
#define BOARD_USB_IND1  BOARD_USB0_IND1
/*----------------------------------------------------------------------------*/
struct Entity;
struct Dfu;
struct DfuBridge;
struct Interface;
struct Interrupt;
struct Timer;
struct TimerFactory;

struct ButtonPackage
{
  struct Interrupt *event;
  struct Timer *timer;
  struct Interrupt *button;
};

struct DfuPackage
{
  struct Timer *timer;
  struct Entity *usb;
  struct Dfu *dfu;
  struct DfuBridge *bridge;
};

struct MemoryPackage
{
  struct Interface *spifi;
  struct Interface *flash;

  struct FlashGeometry geometry[2];
  size_t offset;
  size_t regions;
};

struct TimerPackage
{
  struct TimerFactory *factory;
  struct Timer *timer;
};
/*----------------------------------------------------------------------------*/
void boardClockPostUpdate(void);
void boardResetClock(void);
void boardResetClockPartial(void);
void boardSetupClockExt(void);
void boardSetupClockPll(void);
void boardSetupDefaultWQ(void);
void boardSetupMemoryFlash(struct MemoryPackage *);
void boardSetupMemoryFlashB(struct MemoryPackage *);
void boardSetupMemoryNOR(struct MemoryPackage *);
void boardSetupMemorySRAM(struct MemoryPackage *, void *, size_t);
struct Interface *boardSetupSpim(void);
struct Timer *boardSetupTimer(void);
struct Entity *boardSetupUsb(void);
struct Entity *boardSetupUsb0(void);
struct Entity *boardSetupUsb1(void);

void boardSetupButtonPackage(struct ButtonPackage *, struct TimerFactory *);
void boardSetupDfuPackage(struct DfuPackage *, struct TimerFactory *,
    struct Interface *, struct FlashGeometry *, size_t, size_t, void (*)(void));
void boardSetupTimerPackage(struct TimerPackage *);
/*----------------------------------------------------------------------------*/
#endif /* LPC43XX_DFU_SHARED_BOARD_H_ */
