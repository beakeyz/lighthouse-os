#ifndef __GDT__
#define __GDT__

#include <libk/stddef.h>

#define GDT_NULL_SELC 0
#define GDT_KERNEL_CODE 0x8
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE 0x20
#define GDT_USER_DATA 0x18
#define GDT_TSS_SEL 0x28
#define GDT_TSS_2_SEL 0x30

#define AVAILABLE_TSS 0x9
#define BUSY_TSS 0xb

// TODO: defines for flag bits

typedef struct {
    uint16_t limit;
    uintptr_t base;
} __attribute__((packed)) gdt_pointer_t;

// 64-bit tss =)
typedef struct tss_entry {
	uint32_t reserved_0;
	uint64_t rsp[3]; // 6 uint32_ts with a high and a low component
	uint64_t reserved_1;
	uint64_t ist[7]; // 14 uint32_ts with a high and a low component
	uint64_t reserved_2;
	uint16_t reserved_3;
	uint16_t iomap_base;
} __attribute__ ((packed)) tss_entry_t;

typedef union {
	struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_high;
    uint8_t type: 4;
    uint8_t descriptor_type : 1;
    uint8_t dpl : 2;
    uint8_t segment_present : 1;
    uint8_t limit_high : 4;
    uint8_t unused : 1;
    uint8_t op_size64 : 1;
    uint8_t op_size32 : 1;
    uint8_t granularity : 1;
    uint8_t base_hi2;
	} structured;
  struct {
    uint32_t low;
    uint32_t high;
  };
	uintptr_t raw;
} __attribute__((packed)) gdt_entry_t;

ALWAYS_INLINE uintptr_t get_gdte_base(gdt_entry_t* entry) {
  uintptr_t base = entry->structured.base_low;
  base |= entry->structured.base_high << 16u;
  base |= entry->structured.base_hi2 << 24u;
  return base;
}

ALWAYS_INLINE void set_gdte_base(gdt_entry_t* entry, uintptr_t base) {
  entry->structured.base_low = base & 0xffffu;
  entry->structured.base_hi2 = (base >> 16u) & 0xffu;
  entry->structured.base_hi2 = (base >> 24u) & 0xffu;
}

ALWAYS_INLINE void set_gdte_limit(gdt_entry_t* entry, uint32_t limit) {
  entry->structured.limit_low = limit & 0xffff;
  entry->structured.limit_high = (limit >> 16) & 0xf;
}

#endif // !__GDT__
