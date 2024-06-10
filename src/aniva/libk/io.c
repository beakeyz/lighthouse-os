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
uint32_t in32(uint16_t port)
{
    uint32_t value;
    asm volatile("inl %%dx, %%eax" : "=a"(value) : "dN"(port));
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

void udelay(size_t microseconds)
{
    for (size_t i = 0; i < microseconds; ++i)
        io_delay();
}

/*
 * Memory IO functions
 * TODO: memory barriers and fencing
 */

uint8_t mmio_read_byte(void* address)
{
    return *(volatile uint8_t*)(address);
}

uint16_t mmio_read_word(void* address)
{
    return *(volatile uint16_t*)(address);
}

uint32_t mmio_read_dword(void* address)
{
    return *(volatile uint32_t*)(address);
}

uint64_t mmio_read_qword(void* address)
{
    return *(volatile uint64_t*)(address);
}

void mmio_write_byte(void* address, uint8_t value)
{
    *(volatile uint8_t*)(address) = value;
}

void mmio_write_word(void* address, uint16_t value)
{
    *(volatile uint16_t*)(address) = value;
}

void mmio_write_dword(void* address, uint32_t value)
{
    *(volatile uint32_t*)(address) = value;
}

void mmio_write_qword(void* address, uintptr_t value)
{
    *(volatile uintptr_t*)(address) = value;
}
