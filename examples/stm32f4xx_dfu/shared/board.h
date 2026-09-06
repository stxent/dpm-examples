/*
 * stm32f4xx_dfu/shared/board.h
 * Copyright (C) 2026 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef STM32F4XX_DFU_SHARED_BOARD_H_
#define STM32F4XX_DFU_SHARED_BOARD_H_
/*----------------------------------------------------------------------------*/
#include <halm/generic/flash.h>
/*----------------------------------------------------------------------------*/
#define BOARD_BUTTON      PIN(PORT_A, 0)
#define BOARD_BUTTON_INV  false
#define BOARD_LED_0       PIN(PORT_F, 9)
#define BOARD_LED_1       PIN(PORT_F, 10)
#define BOARD_LED         BOARD_LED_0
#define BOARD_LED_INV     false
/*----------------------------------------------------------------------------*/
struct Dfu;
struct DfuBridge;
struct Interface;
struct Interrupt;
struct Timer;
struct TimerFactory;
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
  struct Interface *lower;
  struct Interface *upper;

  struct FlashGeometry geometry[3];
  size_t offset;
  size_t regions;
};

struct TimerPackage
{
  struct TimerFactory *factory;
  struct Timer *timer;
};
/*----------------------------------------------------------------------------*/
void boardResetClock(void);
void boardSetupClockPll(void);
void boardSetupDefaultWQ(void);
void boardSetupMemoryESRAM(struct MemoryPackage *);
void boardSetupMemoryFlash(struct MemoryPackage *);
struct Interface *boardSetupSpim(void);
struct Timer *boardSetupTimer(void);
struct Usb *boardSetupUsb(void);

void boardSetupButtonPackage(struct ButtonPackage *, struct TimerFactory *);
void boardSetupDfuPackage(struct DfuPackage *, struct TimerFactory *,
    struct Interface *, struct FlashGeometry *, size_t, size_t, void (*)(void));
void boardSetupTimerPackage(struct TimerPackage *);
/*----------------------------------------------------------------------------*/
#endif /* STM32F4XX_DFU_SHARED_BOARD_H_ */
