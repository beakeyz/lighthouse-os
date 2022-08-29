#ifndef __SERIAL__
#define __SERIAL__

#define COM1 0x3F8

void init_serial();

void putch(char c);

void print(const char* str);

void println(const char* str);

#endif // !__SERIAL__
