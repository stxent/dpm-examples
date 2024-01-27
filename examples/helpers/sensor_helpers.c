/*
 * helpers/sensor_helpers.c
 * Copyright (C) 2024 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "sensor_helpers.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
/*----------------------------------------------------------------------------*/
float applyDataFormatFloat(int32_t raw, const DataFormat *format)
{
  return (float)raw / (1 << format->q);
}
/*----------------------------------------------------------------------------*/
DecimalNumber applyDataFormatDecimal(int32_t raw, const DataFormat *format,
    unsigned int multiplier)
{
  DecimalNumber result;

  result.negative = raw < 0;
  const int32_t absolute = result.negative ? -raw : raw;

  result.integer = absolute / (1 << format->q);
  result.decimal = absolute & ((1 << format->q) - 1);
  result.decimal = result.decimal * multiplier / (1 << format->q);

  return result;
}
/*----------------------------------------------------------------------------*/
DataFormat parseDataFormat(const char *str)
{
  enum State
  {
    STATE_FIRST,
    STATE_I_VALUE,
    STATE_Q_VALUE
  } state = STATE_FIRST;

  DataFormat format = {0, 0, 0};
  DataFormat result = {0, 0, 0};
  bool error = false;

  do
  {
    bool copy = false;

    switch (*str)
    {
      case '\0':
        copy = true;
        break;

      case 'i':
        if (state != STATE_FIRST)
          copy = true;

        state = STATE_I_VALUE;
        break;

      case 'q':
        if (state != STATE_FIRST && state != STATE_I_VALUE)
          copy = true;

        state = STATE_Q_VALUE;
        break;

      default:
        if (*str >= '0' && *str <= '9')
        {
          if (state == STATE_I_VALUE)
          {
            format.i = format.i * 10 + (*str - '0');
          }
          else if (state == STATE_Q_VALUE)
          {
            format.q = format.q * 10 + (*str - '0');
          }
          else
            error = true;
        }
        else
          error = true;
        break;
    }

    if (copy)
    {
      if (result.n)
      {
        if (result.i != format.i || result.q != format.q)
          error = true;
      }
      else
      {
        result.i = format.i;
        result.q = format.q;
      }

      ++result.n;
      format.i = 0;
      format.q = 0;
    }
  }
  while (!error && *str++);

  return error ? (DataFormat){0, 0, 0} : result;
}
/*----------------------------------------------------------------------------*/
/**
 * Convert packed integer values from an IQ format to a string.
 * @param values Pointer to an array of packed values, array type will be
 * deduced automatically from the format descriptor.
 * @param format Format descriptor of an input array.
 * @param integerFormatSign When @b true sign will be added to the integer part.
 * @param decimalFormatPrecision Number of digits in a fractional part.
 * @param output Buffer for an output data.
 * @return Number of characters printed.
 */
size_t printFormattedValues(const void *values, const DataFormat *format,
    bool integerFormatSign, unsigned int decimalFormatPrecision, char *output)
{
  static const unsigned int PRECISION_MUL[] = {
      1, 10, 100, 1000, 10000, 100000, 1000000
  };

  assert(values != NULL && format != NULL && output != NULL);
  assert(decimalFormatPrecision <= 6);

  const unsigned int mul = PRECISION_MUL[decimalFormatPrecision];
  const unsigned int width = format->i + format->q;
  assert(width == 8 || width == 16 || width == 32);

  const char decimalFormat[] = {
      '%', '0', '0' + decimalFormatPrecision, 'i', '\0'
  };

  size_t processed = 0;

  for (size_t index = 0; index < format->n; ++index)
  {
    const int32_t value =
        (width == 8) ? *((const int8_t *)values + index)
        : (width == 16) ? *((const int16_t *)values + index)
        : *((const int32_t *)values + index);

    const DecimalNumber converted = applyDataFormatDecimal(value, format, mul);

    /* Integer part */
    if (integerFormatSign)
      output[processed++] = converted.negative ? '-' : ' ';
    processed += sprintf(output + processed, "%i", converted.integer);

    /* Fractional part */
    if (decimalFormatPrecision)
    {
      output[processed++] = '.';
      processed += sprintf(output + processed, decimalFormat,
          converted.decimal);
    }

    /* Element separator */
    if (index < (size_t)format->n - 1)
      output[processed++] = ' ';
  }

  return processed;
}
