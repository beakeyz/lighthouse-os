#ifndef __KMEM_MANAGER__
#define __KMEM_MANAGER__

#include <arch/x86/multiboot.h>
#include <libc/stddef.h>
#include "pml.h"

// some faultcodes
#define PRESENT_VIOLATION   0x1
#define WRITE_VIOLATION 0x2
#define ACCESS_VIOLATION    0x4
#define RESERVED_VIOLATION   0x8
#define FETCH_VIOLATION 0x10

// paging masks
#define KRNL_HEAP_START 0xFFFFff0000000000UL
#define HIGH_MAP_BASE   0xFFFFff8000000000UL
#define MMIO_BASE       0xFFFFff1fc0000000UL
#define USR_DEV_MAP     0x0000400000000000UL

#define PAGE_SIZE_MASK  0xFFFFffffFFFFf000UL
#define PAGE_LOW_MASK   0x0000000000000FFFUL
#define ENTRY_MASK      0x1FF

#define PAGE_SIZE 0x200000
#define SMALL_PAGE_SIZE 0x1000UL
#define PAGE_SIZE_BYTES 0x200000UL

// defines for alignment
#define ALIGN_UP(addr, size) \
    ((addr % size == 0) ? (addr) : (addr) + size - ((addr) % size))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % size))

// defines for bitmap
#define INDEX_FROM_BIT(b)  ((b) >> 5)
#define OFFSET_FROM_BIT(b) ((b) & 0x1F)

typedef struct {
    uint8_t type;
    uint64_t start;
    size_t length;
} phys_mem_range_t;

typedef struct {
    uint64_t upper;
    uint64_t lower;
} contiguous_phys_virt_range_t; 

typedef struct {
    uint32_t mmap_entry_num;
    multiboot_memory_map_t* mmap_entries;
    uint8_t reserved_phys_count;
} kmem_data_t;

void init_kmem_manager (uint32_t mb_addr, uintptr_t mb_first_addr, uintptr_t first_valid_alloc_addr);
void prep_mmap (struct multiboot_tag_mmap* mmap);

void parse_memmap ();

void* kmem_from_phys (uintptr_t addr);
void kmem_mark_frame (uintptr_t frame);

#endif // !__KMEM_MANAGER__
