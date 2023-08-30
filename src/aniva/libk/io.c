#include "io.h"

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
uint32_t in32(uint16_t port) {
  uint32_t value;
  asm volatile ("inl %%dx, %%eax" : "=a" (value) : "dN" (port));
  return value;
}

void out8(uint16_t port, uint8_t value)
{
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

void out16(uint16_t port, uint16_t value)
{
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

void out32(uint16_t port, uint32_t value)
{
    asm volatile("outl %%eax, %%dx" : : "a"(value), "Nd"(port));
}

/*!
 * @brief Roughly 1 us delay
 *
 * This is the exact same thing linux uses, so it'll be fine prob
 */
void io_delay()
{
  const uint16_t delay_port = 0x80;
  out8(delay_port, 0);
}

void delay(size_t microseconds)
{
    for (size_t i = 0; i < microseconds; ++i)
      io_delay();
}
