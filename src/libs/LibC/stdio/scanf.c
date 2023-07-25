

#include "stdarg.h"
#include <stdio.h>

int real_va_sscanf(const char* buffer, const char* fmt, va_list args)
{

  if (!buffer || !fmt)
    return -1;

  while (*fmt) {

    if (*fmt == '%') {
      fmt++;

      switch (*fmt) {
        case 'd':
          {
            int number = 0;

            while (*buffer && *buffer >= '0' && *buffer <= '9') {
              number *= 10;
              number += *buffer - '0';

              buffer++;
            }

            int* out_addr = va_arg(args, int*);

            *out_addr = number;
          }
          break;
      }
    }

    fmt++;
    buffer++;
  }
  
  return 0;
}

int real_va_scanf(FILE* stream, const char* fmt, va_list args)
{
  const char* c;
  int result;

  if (!stream || !fmt)
    return -1;

  result = 0;
  c = fmt;

  for (; *c; c++) {
    if (*c != '%') {
      putchar(*c);
      continue;
    }
  }

  return result;
}
