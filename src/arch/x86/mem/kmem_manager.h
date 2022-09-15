#ifndef __KMEM_MANAGER__
#define __KMEM_MANAGER__
#include <arch/x86/multiboot.h>
#include <libc/stddef.h>
#include "libc/linkedlist.h"

#define PML4_ENTRY(address)((address>>39) & 0x1ff)
#define PDPR_ENTRY(address)((address>>30) & 0x1ff)
#define PD_ENTRY(address)((address>>21) & 0x1ff)
#define PT_ENTRY(address)((address>>12) & 0x1ff)
#define SIGN_EXTENSION 0xFFFF000000000000
#define ENTRIES_TO_ADDRESS(pml4, pdpr, pd, pt)((pml4 << 39) | (pdpr << 30) | (pd << 21) |  (pt << 12))

#define PAGE_ALIGNMENT_MASK (PAGE_SIZE_IN_BYTES-1)

#define ALIGN_PHYSADDRESS(address)(address & (~(PAGE_ALIGNMENT_MASK)))

#define HIGHER_HALF_ADDRESS_OFFSET 0xFFFF800000000000

#define PRESENT_BIT 1
#define WRITE_BIT 0b10
#define HUGEPAGE_BIT 0b10000000

#define VM_PAGES_PER_TABLE 0x200

#define PRESENT_VIOLATION   0x1
#define WRITE_VIOLATION 0x2
#define ACCESS_VIOLATION    0x4
#define RESERVED_VIOLATION   0x8
#define FETCH_VIOLATION 0x10

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

// defines for alignment
#define ALIGN_UP(addr, size) \
    ((addr % size == 0) ? (addr) : (addr) + size - ((addr) % size))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % size))

void init_kmem_manager (uint32_t mb_addr, uint32_t mb_first_addr, struct multiboot_tag_basic_meminfo* basic_info);
void prep_mmap (struct multiboot_tag_mmap* mmap);
void init_mmap(struct multiboot_tag_basic_meminfo* basic_info);

void parse_memmap ();

void* alloc_frame ();

void* free_frame ();

// phys to virt mappings

void* get_vaddr (void* vaddr, int flags);
void* phys_to_virt (void* phys, void* virt, int flags);

void clear_table(uintptr_t* table);
uint64_t ensure_address_in_higher_half( uint64_t address );
bool is_address_higher_half(uint64_t address);

#endif // !__KMEM_MANAGER__
