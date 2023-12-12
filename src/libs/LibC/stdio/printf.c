
#include "stdarg.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int __write_byte(FILE* stream, uint64_t* counter, char byte);
extern int __write_bytes(FILE* stream, uint64_t* counter, char* bytes);

static int __write_decimal(uint64_t* counter, FILE* stream, uint64_t value, int prec)
{
  char tmp[128];
  uint8_t size = 1;

  memset(tmp, '0', sizeof(tmp));

  /* Test the size of this value */
  uint64_t size_test = value;
  while (size_test / 10 > 0) {
      size_test /= 10;
      size++;
  }

  if (size < prec && prec && prec != -1)
    size = prec;

  uint8_t index = 0;
  
  while (value / 10 > 0) {
      uint8_t remain = value % 10;
      value /= 10;
      tmp[size - 1 - index] = remain + '0';
      index++;
  }
  uint8_t remain = value % 10;
  tmp[size - 1 - index] = remain + '0';
  //tmp[size] = 0;

  //__write_bytes(stream, counter, tmp);

  /* NOTE: don't use __write_bytes here, since it simply goes until it finds a NULL-byte */
  for (uint32_t i = 0; i < size; i++) {
    __write_byte(stream, counter, tmp[i]);
  }

  return size;
}

int real_va_sprintf(uint8_t mode, FILE* stream, const char* fmt, va_list va)
{
  int prec;
  uint64_t i = 0;
  int64_t value;
  uint32_t decimal_size;

  for (const char* c = fmt; *c; c++) {

    decimal_size = 0;
    value = 0;
    prec = -1;

    if (*c != '%') {
      __write_byte(stream, &i, *c);
      continue;
    }

    /* We are dealing with a format character skip */
    c++;

    /* TODO: support entire format */

    if (*c == '.') {
      prec = 0;
      c++;

      while (isdigit(*c)) {
        prec *= 10;
        prec += *c - '0';
        c++;
      }
    }

    /* This is hilarious */
    while (*c == 'l') {
      decimal_size++;
      c++;
      /* %ll is the max */
      if (decimal_size == 2)
        break;
    }

    switch(*c) {
      case 'c':
        __write_byte(stream, &i, va_arg(va, int));
        break;
      case 's':
        /*
         * This is our own version of __write_bytes, since we want to stop when we 
         * encounter a NULL-byte
         */
        for (char* c = va_arg(va, char*);; c++) {

          if (!*c)
            break;

          if (__write_byte(stream, &i, *c) < 0)
            break;
        }
        break;
      case 'i':
      case 'd':
        {
          switch (decimal_size) {
            case 0:
              value = va_arg(va, int);
              break;
            case 1:
              value = va_arg(va, long);
              break;
            case 2:
              value = va_arg(va, long long);
              break;
          }

          if (value < 0) {
            value = -value;
            __write_byte(stream, &i, '-');
          }

          __write_decimal(&i, stream, value, prec);
          break;
        }
      case 'X':
      case 'x':
        {
          __write_bytes(stream, &i, "TODO:hex =)");
          break;
        }
      case '%':
        __write_byte(stream, &i, '%');
        break;
      default:
        //exit_noimpl("real_va_sprintf: edgecase");
        __write_byte(stream, &i, *c);
        break;
    }
  }

  __write_byte(stream, NULL, NULL);
  /* Return the length of everything we wrote */
  return i;
}
