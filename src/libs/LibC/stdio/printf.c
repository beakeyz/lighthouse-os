
#include "stdarg.h"
#include <stdio.h>
#include <stdlib.h>

extern int __write_byte(FILE* stream, uint64_t* counter, char byte);
extern int __write_bytes(FILE* stream, uint64_t* counter, char* bytes);

int real_va_sprintf(uint8_t mode, FILE* stream, const char* fmt, va_list va)
{
  uint64_t i = 0;

  for (const char* c = fmt; *c; c++) {
    if (*c != '%') {
      __write_byte(stream, &i, *c);
      continue;
    }

    /* We are dealing with a format character skip */
    c++;

    /* TODO: support entire format */

    switch(*c) {
      case 'c':
        __write_byte(stream, &i, va_arg(va, int));
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

  /* Return the length of everything we wrote */
  return i;
}
