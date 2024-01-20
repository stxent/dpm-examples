/*
 * helpers/sensor_helpers.h
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the MIT License
 */

#ifndef HELPERS_SENSOR_HELPERS_H_
#define HELPERS_SENSOR_HELPERS_H_
/*----------------------------------------------------------------------------*/
#include <xcore/helpers.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/*----------------------------------------------------------------------------*/
typedef struct
{
  uint8_t i;
  uint8_t q;
  uint8_t n;
} DataFormat;

typedef struct
{
  int integer;
  int decimal;
  bool negative;
} DecimalNumber;
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

DecimalNumber applyDataFormatDecimal(int32_t, const DataFormat *, unsigned int);
float applyDataFormatFloat(int32_t, const DataFormat *);
DataFormat parseDataFormat(const char *);
size_t printFormattedValues(const void *, const DataFormat *, const char *,
    const char *, char *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* HELPERS_SENSOR_HELPERS_H_ */
