
#include "stdarg.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern int __write_byte(FILE* stream, uint64_t* counter, char byte);
extern int __write_bytes(FILE* stream, uint64_t* counter, char* bytes);

static int __write_decimal(uint64_t* counter, FILE* stream, uint64_t value) 
{
  char tmp[128];

  uint8_t size = 0;
  uint64_t size_test = value;
  while (size_test / 10 > 0) {
      size_test /= 10;
      size++;
  }

  uint8_t index = 0;
  
  while (value / 10 > 0) {
      uint8_t remain = value % 10;
      value /= 10;
      tmp[size - index] = remain + '0';
      index++;
  }
  uint8_t remain = value % 10;
  tmp[size - index] = remain + '0';
  tmp[size + 1] = 0;

  __write_bytes(stream, counter, tmp);

  return size;
}

int real_va_sprintf(uint8_t mode, FILE* stream, const char* fmt, va_list va)
{
  uint64_t i = 0;
  int64_t value;
  uint32_t decimal_size;

  for (const char* c = fmt; *c; c++) {

    decimal_size = 0;
    value = 0;

    if (*c != '%') {
      __write_byte(stream, &i, *c);
      continue;
    }

    /* We are dealing with a format character skip */
    c++;

    /* TODO: support entire format */

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
        __write_bytes(stream, &i, va_arg(va, char*));
        break;
      case 'i':
      case 'd':
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

        __write_decimal(&i, stream, value);
        break;
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
        __write_byte(stream, &i, *c);
        break;
    }
  }

  __write_byte(stream, NULL, NULL);
  /* Return the length of everything we wrote */
  return i;
}
