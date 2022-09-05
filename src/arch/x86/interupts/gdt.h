#include <libc/stddef.h>

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

typedef struct {
    uint32_t base_high;
    uint32_t reserved;
} __attribute__((packed)) gdt_entry_high_t;

typedef struct {
    uint32_t reserved_0;
	uint64_t rsp[3];
	uint64_t reserved_1;
	uint64_t ist[7];
	uint64_t reserved_2;
	uint16_t reserved_3;
	uint16_t iomap_base;
} __attribute__((packed)) tss_entry_t;

typedef struct 
{
    gdt_entry_t default_entries[6];
    gdt_entry_high_t high_entry;
    gdt_pointer_t ptr;
    tss_entry_t tss;
    
} __attribute__((packed)) gdt_struct_t;


void setup_gdt();
