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

typedef struct tss_entry {
	uint32_t reserved_0;
	uint64_t rsp[3];
	uint64_t reserved_1;
	uint64_t ist[7];
	uint64_t reserved_2;
	uint16_t reserved_3;
	uint16_t iomap_base;
} __attribute__ ((packed)) tss_entry_t;

typedef struct {
        uint16_t limit_low;
	    uint16_t base_low;
	    uint8_t base_middle;
	    uint8_t access;
	    uint8_t granularity;
	    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
	uint32_t base_highest;
	uint32_t reserved0;
} __attribute__((packed)) gdt_entry_high_t;

typedef struct  {
    gdt_entry_t null;
    gdt_entry_t kernel_code;
    gdt_entry_t kernel_data;
    gdt_entry_t user_null;
    gdt_entry_t user_code;
    gdt_entry_t user_data;
} __attribute__((packed)) __attribute__((aligned(0x1000))) _gdt_struct_t;

void setup_gdt();
extern void load_gdt (gdt_pointer_t* ptr);
#endif // !__GDT__
