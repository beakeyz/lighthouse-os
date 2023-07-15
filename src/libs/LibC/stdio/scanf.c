

#include "stdarg.h"
#include <stdio.h>

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
