/*
 * helpers/display_helpers.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef HELPERS_DISPLAY_HELPERS_H_
#define HELPERS_DISPLAY_HELPERS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/helpers.h>
#include <stddef.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
typedef struct
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
} Color;

struct Interface;
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

Color interpolateColor(Color a, Color b, int current, int total);
Color makeColor(unsigned int);
uint16_t rgbTo565(Color);

void handleChessFill(struct Interface *, unsigned int, unsigned int,
    void *, size_t);
void handleGradientFill(struct Interface *, unsigned int, unsigned int,
    void *, size_t);
void handleLineFill(struct Interface *, unsigned int, unsigned int,
    void *, size_t);
void handleMarkerFill(struct Interface *, unsigned int, unsigned int,
    void *, size_t);
void handleSolidFill(struct Interface *, unsigned int, unsigned int,
    void *, size_t);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* HELPERS_DISPLAY_HELPERS_H_ */
