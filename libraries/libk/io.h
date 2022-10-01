#ifndef __IO__
#define __IO__
#include <libk/stddef.h>


uint8_t in8(uint16_t port);

uint16_t in16(uint16_t port);

/*
uint32_t in32(uint16_t port)
{
    uint32_t value;
    asm volatile("inl %1, %0"
                 : "=a"(value)
                 : "Nd"(port));
    return value;
}
*/

void out8(uint16_t port, uint8_t value);

void out16(uint16_t port, uint16_t value);

/*
void out32(uint16_t port, uint32_t value)
{
    asm volatile("outl %0, %1" ::"a"(value), "Nd"(port));
}*/

void delay(size_t microseconds);

#endif // !DEBUG
