#ifndef __IO__
#define __IO__
#include <libc/stddef.h>

#define asm __asm__

uint8_t in8(uint16_t port)
{
    uint8_t value;
    asm volatile("inb %1, %0"
                 : "=a"(value)
                 : "Nd"(port));
    return value;
}

uint16_t in16(uint16_t port)
{
    uint16_t value;
    asm volatile("inw %1, %0"
                 : "=a"(value)
                 : "Nd"(port));
    return value;
}

uint32_t in32(uint16_t port)
{
    uint32_t value;
    asm volatile("inl %1, %0"
                 : "=a"(value)
                 : "Nd"(port));
    return value;
}

void out8(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" ::"a"(value), "Nd"(port));
}

void out16(uint16_t port, uint16_t value)
{
    asm volatile("outw %0, %1" ::"a"(value), "Nd"(port));
}

void out32(uint16_t port, uint32_t value)
{
    asm volatile("outl %0, %1" ::"a"(value), "Nd"(port));
}

void delay(size_t microseconds)
{
    for (size_t i = 0; i < microseconds; ++i)
        in8(0x80);
}

#endif // !DEBUG
