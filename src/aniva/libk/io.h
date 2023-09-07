#ifndef __ANIVA_IO__
#define __ANIVA_IO__

#include <libk/stddef.h>

uint8_t in8(uint16_t port);
uint16_t in16(uint16_t port);
uint32_t in32(uint16_t port);
void out8(uint16_t port, uint8_t value);
void out16(uint16_t port, uint16_t value);
void out32(uint16_t port, uint32_t value);

void io_delay();
void delay(size_t microseconds);

uint8_t mmio_read_byte(void* address);
uint16_t mmio_read_word(void* address);
uint32_t mmio_read_dword(void* address);
uint64_t mmio_read_qword(void* address);

void mmio_write_byte(void* address, uint8_t value);
void mmio_write_word(void* address, uint16_t value);
void mmio_write_dword(void* address, uint32_t value);
void mmio_write_qword(void* address, uintptr_t value);

#endif // !DEBUG
