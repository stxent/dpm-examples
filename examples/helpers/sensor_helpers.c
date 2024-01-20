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
static unsigned int extractFormatWidth(const char *);
static bool isSignRequired(const char *);
/*----------------------------------------------------------------------------*/
static unsigned int extractFormatWidth(const char *format)
{
  unsigned int width = 0;

  do
  {
    if (*format >= '0' && *format <= '9')
    {
      if (width)
        width = width * 10 + (*format - '0');
      else if (*format != '0')
        width = *format - '0';
    }
    else if (width)
    {
      break;
    }
  }
  while (*format++);

  return width;
}
/*----------------------------------------------------------------------------*/
static bool isSignRequired(const char *format)
{
  do
  {
    if (*format == 'c')
      return true;
  }
  while (*format++);

  return false;
}
/*----------------------------------------------------------------------------*/
float applyDataFormatFloat(int32_t raw, const DataFormat *format)
{
  return (float)raw / (1 << format->q);
}
/*----------------------------------------------------------------------------*/
DecimalNumber applyDataFormatDecimal(int32_t raw, const DataFormat *format,
    unsigned int precision)
{
  DecimalNumber result;
  int mul = 1;

  while (precision)
  {
    mul *= 10;
    --precision;
  }

  result.negative = raw < 0;
  const int32_t absolute = result.negative ? -raw : raw;

  result.integer = absolute / (1 << format->q);
  result.decimal = absolute & ((1 << format->q) - 1);
  result.decimal = result.decimal * mul / (1 << format->q);

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
size_t printFormattedValues(const void *values, const DataFormat *format,
    const char *integerFormat, const char *decimalFormat, char *output)
{
  assert(values != NULL && format != NULL && output != NULL);
  assert(integerFormat != NULL);

  const unsigned int width = format->i + format->q;
  assert(width == 8 || width == 16 || width == 32);

  const bool sign = isSignRequired(integerFormat);
  const unsigned int precision = decimalFormat != NULL ?
      extractFormatWidth(decimalFormat) : 0;
  size_t processed = 0;

  for (size_t index = 0; index < format->n; ++index)
  {
    const int32_t value =
        (width == 8) ? *((const int8_t *)values + index)
        : (width == 16) ? *((const int16_t *)values + index)
        : *((const int32_t *)values + index);

    const DecimalNumber converted =
        applyDataFormatDecimal(value, format, precision);

    if (sign)
    {
      processed += sprintf(output + processed, integerFormat,
          converted.negative ? '-' : ' ', converted.integer);
    }
    else
    {
      processed += sprintf(output + processed, integerFormat,
          converted.integer);
    }

    if (decimalFormat != NULL)
    {
      output[processed++] = '.';
      processed += sprintf(output + processed, decimalFormat,
          converted.decimal);
    }

    if (index < (size_t)format->n - 1)
      output[processed++] = ' ';
  }

  return processed;
}
