#ifndef __SERIAL__
#define __SERIAL__
#include <libk/stddef.h>

#define COM1 (uint16_t)0x3F8

void init_serial();

int serial_putch(char c);
int serial_print(const char* str);
int serial_println(const char* str);

#endif // !__SERIAL__
