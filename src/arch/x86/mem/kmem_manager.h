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

#define PDP_MASK 0x3fffffffUL
#define  PD_MASK 0x1fffffUL
#define  PT_MASK PAGE_LOW_MASK

// page flags
#define KMEM_FLAG_KERNEL       0x01
#define KMEM_FLAG_WRITABLE     0x02
#define KMEM_FLAG_NOCACHE      0x04
#define KMEM_FLAG_WRITETHROUGH 0x08
#define KMEM_FLAG_SPEC         0x10
#define KMEM_FLAG_WC           (KMEM_FLAG_NOCACHE | KMEM_FLAG_WRITETHROUGH | KMEM_FLAG_SPEC)
#define KMEM_FLAG_NOEXECUTE    0x20

#define KMEM_GET_MAKE 0x01

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
uintptr_t kmem_to_phys (pml_t* root, uintptr_t addr);
void kmem_mark_frame_used (uintptr_t frame);
void kmem_mark_frame_free (uintptr_t frame);
void kmem_mark_frame (uintptr_t frame, bool value);


uintptr_t kmem_get_frame ();
pml_t* kmem_get_krnl_dir ();
pml_t* kmem_get_page (uintptr_t addr, unsigned int flags);
void kmem_set_page_flags (pml_t* page, unsigned int flags);

void* kmem_alloc (size_t size);


#endif // !__KMEM_MANAGER__
