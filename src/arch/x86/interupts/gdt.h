#ifndef __GDT__
#define __GDT__

#include <libc/stddef.h>

#define NULL_SELC 0
#define KERNEL_CODE 0x8
#define KERNEL_DATA 0x10
#define USER_CODE 0x23
#define USER_DATA 0x1B

// TODO: defines for flag bits

typedef struct {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed)) gdt_pointer_t;

typedef struct {
    uint16_t limit_low;
	uint16_t base_low;
	uint8_t base_middle;
	uint8_t access;
	uint8_t granularity;
	uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct 
{
    gdt_entry_t default_entries[6];
    gdt_pointer_t ptr;
    
} __attribute__((packed)) gdt_struct_t;


void setup_gdt();
extern void load_gdt (void* ptr);
#endif // !__GDT__
