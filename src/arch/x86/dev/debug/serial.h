#ifndef __SERIAL__
#define __SERIAL__
#include <libc/stddef.h>

#define COM1 (uint16_t)0x3F8

void init_serial();

void putch(char c);

void print(const char* str);

void println(const char* str);

#endif // !__SERIAL__
