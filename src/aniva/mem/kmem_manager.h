#ifndef __KMEM_MANAGER__
#define __KMEM_MANAGER__

#include <libk/multiboot.h>
#include <libk/stddef.h>
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/page_dir.h"
#include "mem/pg.h"

// some faultcodes
#define PRESENT_VIOLATION       0x1
#define WRITE_VIOLATION         0x2
#define ACCESS_VIOLATION        0x4
#define RESERVED_VIOLATION      0x8
#define FETCH_VIOLATION         0x10


// Kernel high virtual base
#define HIGH_MAP_BASE           0xffffffff80000000ULL
// Base for early kernelheap mappings 
#define EARLY_KERNEL_HEAP_BASE  ALIGN_UP((uintptr_t)&_kernel_end, SMALL_PAGE_SIZE)
// Base for early multiboot fb
#define EARLY_FB_MAP_BASE       0xFFFFFFFF10000000ULL
// Base for the quickmap engine. We take the pretty much highest possible vaddr
#define QUICKMAP_BASE           0xFFFFffffFFFF0000ULL

/* Bottom of the userstack per process. Every thread gets thread_id * STACK_SIZE as a new base */
#define HIGH_STACK_BASE         0x0000800000000000ULL

/* We need to be carefull, because the userstack is placed directly under the kernel */
#define THREAD_ENTRY_BASE       0xFFFFFFFF00000000ULL

// paging masks
#define PAGE_SIZE_MASK          0xFFFFffffFFFFf000UL
#define PAGE_LOW_MASK           0xFFFUL
#define ENTRY_MASK              0x1FFUL

#define PAGE_SIZE               0x200000
#define PAGE_SHIFT              12
#define SMALL_PAGE_SIZE         (1 << PAGE_SHIFT) 
#define PAGE_SIZE_BYTES         0x200000UL

#define PDP_MASK                0x3fffffffUL
#define PD_MASK                 0x1fffffUL
#define PT_MASK                 PAGE_LOW_MASK

// page flags
#define KMEM_FLAG_KERNEL        0x01
#define KMEM_FLAG_WRITABLE      0x02
#define KMEM_FLAG_NOCACHE       0x04
#define KMEM_FLAG_WRITETHROUGH  0x08
#define KMEM_FLAG_SPEC          0x10
#define KMEM_FLAG_WC            (KMEM_FLAG_NOCACHE | KMEM_FLAG_WRITETHROUGH | KMEM_FLAG_SPEC)
#define KMEM_FLAG_DMA           (KMEM_FLAG_NOCACHE | KMEM_FLAG_WRITABLE)
#define KMEM_FLAG_NOEXECUTE     0x20
#define KMEM_FLAG_GLOBAL        0x21

// Custom mapping flags
#define KMEM_CUSTOMFLAG_GET_MAKE            0x00000001
#define KMEM_CUSTOMFLAG_CREATE_USER         0x00000002
#define KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE 0x00000004
#define KMEM_CUSTOMFLAG_IDENTITY            0x00000008
#define KMEM_CUSTOMFLAG_NO_REMAP            0x00000010
#define KMEM_CUSTOMFLAG_GIVE_PHYS           0x00000020
#define KMEM_CUSTOMFLAG_USE_QUICKMAP        0x00000040
#define KMEM_CUSTOMFLAG_UNMAP               0x00000080
#define KMEM_CUSTOMFLAG_NO_PHYS_REALIGN     0x00000100
#define KMEM_CUSTOMFLAG_NO_MARK             0x00000200

#define KMEM_STATUS_FLAG_DONE_INIT          0x00000001
#define KMEM_STATUS_FLAG_HAS_QUICKMAP       0x00000002

// defines for alignment
#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

typedef enum {
  PMRT_USABLE = 1,
  PMRT_RESERVED,
  PMRT_ACPI_RECLAIM,
  PMRT_ACPI_NVS,
  PMRT_BAD_MEM,
  PMRT_UNKNOWN,
} PhysMemRangeType_t;

typedef struct {
  PhysMemRangeType_t type;
  uint64_t start;
  size_t length;
} phys_mem_range_t;

typedef struct {
  uint64_t upper;
  uint64_t lower;
} contiguous_phys_virt_range_t; 

/*
 * TODO: encorperate some system that ensures safety with 
 * this structure, like only allowing the owner pointer
 * to destroy this structure and zeroing all the other 
 * m_references when it eventually does
 */
struct kmem_region {
  size_t m_page_count;
  bool m_contiguous;
  page_dir_t* m_dir;
  
  void** m_references;
};
typedef struct kmem_region kregion_t;
typedef struct kmem_region* kregion;

/*
 * This struct represents a page that was gathered from a specific 
 * page dir. We can trace back and verify the pbase if we walk the 
 * page dir with the p0-3 and optionally p4 if we use 5-level paging
 */
struct kmem_page {
  uintptr_t p0, p1, p2, p3, p4;
  vaddr_t vbase;
  paddr_t pbase;
  page_dir_t* dir;
};

typedef struct kmem_mapping {
  size_t m_size;
  vaddr_t m_vbase;
  paddr_t m_pbase;
} kmapping_t;

/*
 * initialize kernel pagetables and physical allocator
 */
void init_kmem_manager(uintptr_t* mb_addr, uintptr_t mb_first_addr, uintptr_t first_valid_alloc_addr);

void protect_kernel();
void protect_heap();

/*
 * extract data from the mmap that gets provided by the
 * bootloader
 */
void prep_mmap (struct multiboot_tag_mmap* mmap);

/*
 * parse the entire mmap
 */
void parse_mmap ();

void kmem_load_page_dir(uintptr_t dir, bool __disable_interupts);

uintptr_t kmem_get_page_idx (uintptr_t page_addr);
uintptr_t kmem_get_page_base (uintptr_t base);
uintptr_t kmem_set_page_base(pml_entry_t* entry, uintptr_t page_base);
uintptr_t kmem_get_page_addr (uintptr_t page_idx);

/*
 * translate a physical address to a virtual address in the
 * kernel pagetables
 */
vaddr_t kmem_ensure_high_mapping (uintptr_t addr);

vaddr_t kmem_from_phys (uintptr_t addr, vaddr_t vbase);

/*
 * translate a virtual address to a physical address in
 * a pagetable given by the caller
 */
uintptr_t kmem_to_phys(pml_entry_t* root, uintptr_t addr);
uintptr_t kmem_to_phys_aligned(pml_entry_t* root, uintptr_t addr);

void kmem_set_phys_page_used (uintptr_t idx);
void kmem_set_phys_page_free (uintptr_t idx);

void kmem_set_phys_range_used(uintptr_t start_idx, size_t page_count);
void kmem_set_phys_range_free(uintptr_t start_idx, size_t page_count);

void kmem_set_phys_page (uintptr_t idx, bool value);
bool kmem_is_phys_page_used (uintptr_t idx);

/*
 * flush tlb for a virtual address
 */
void kmem_invalidate_tlb_cache_entry(uintptr_t vaddr);
void kmem_invalidate_tlb_cache_range(uintptr_t vaddr, size_t size);
void kmem_refresh_tlb();

/*
 * completely flush the tlb cache
 */
void kmem_flush_tlb();

ErrorOrPtr kmem_request_physical_page();
ErrorOrPtr kmem_prepare_new_physical_page();
ErrorOrPtr kmem_return_physical_page(paddr_t page_base);
pml_entry_t* kmem_get_krnl_dir();

pml_entry_t* kmem_get_page(pml_entry_t* root, uintptr_t addr, uint32_t kmem_flags, uint32_t page_flags);
pml_entry_t* kmem_get_page_with_quickmap(pml_entry_t* table, vaddr_t virt, uint32_t kmem_flags, uint32_t page_flags);

/* TODO: implement */
pml_entry_t* kmem_clone_page(pml_entry_t* page);

void kmem_set_page_flags(pml_entry_t* page,  uint32_t flags);

/* mem mapping */

/*
 * Generic mapping functions
 */
bool kmem_map_page (pml_entry_t* table, uintptr_t virt, uintptr_t phys, uint32_t kmem_flags, uint32_t page_flags);
bool kmem_map_range (pml_entry_t* table, uintptr_t virt_base, uintptr_t phys_base, size_t page_count, uint32_t kmem_flags, uint32_t page_flags);
ErrorOrPtr kmem_map_into(pml_entry_t* table, vaddr_t old, vaddr_t new, size_t size, uint32_t kmem_flags, uint32_t page_flags);
ErrorOrPtr kmem_copy_kernel_mapping(pml_entry_t* new_table);
bool kmem_unmap_page(pml_entry_t* table, uintptr_t virt);
bool kmem_unmap_page_ex(pml_entry_t* table, uintptr_t virt, uint32_t custom_flags);
bool kmem_unmap_range(pml_entry_t* table, uintptr_t virt, size_t page_count);

/*
 * initialize the physical pageframe allocator
 */
void kmem_init_physical_allocator();

/*
 * These functions are all about mapping and allocating
 * memory using the global physical memory bitmap
 */
ErrorOrPtr __kmem_kernel_alloc(uintptr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags);
ErrorOrPtr __kmem_kernel_alloc_range (size_t size, uint32_t custom_flags, uint32_t page_flags);

ErrorOrPtr __kmem_alloc(pml_entry_t* map, paddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags);
ErrorOrPtr __kmem_alloc_ex(pml_entry_t* map, paddr_t addr, vaddr_t vbase, size_t size, uint32_t custom_flags, uintptr_t page_flags);

ErrorOrPtr __kmem_alloc_range(pml_entry_t* map, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags);

ErrorOrPtr __kmem_dealloc(pml_entry_t* map, uintptr_t virt_base, size_t size);
ErrorOrPtr __kmem_kernel_dealloc(uintptr_t virt_base, size_t size);

ErrorOrPtr __kmem_map_and_alloc_scattered(pml_entry_t* map, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags);

/*
 * Return a translation of the address provided that is 
 * useable for the kernel
 * 
 * Fails if virtual_address is not mapped into the provided map
 */
ErrorOrPtr kmem_get_kernel_address(vaddr_t virtual_address, pml_entry_t* from_map);

ErrorOrPtr kmem_to_current_pagemap(vaddr_t vaddr, pml_entry_t* external_map);

/* access to kmem_manager data struct */
list_t const* kmem_get_phys_ranges_list();

/*
 * Prepares a new pagemap that has virtual memory mapped from 0 -> initial_size
 * This can act as clean userspace directory creation, though a lot is still missing
 */
page_dir_t kmem_create_page_dir(uint32_t custom_flags, size_t initial_size);

/*
 * Free this entire addressspace for future use
 */
void kmem_destroy_page_dir(pml_entry_t* dir);

// TODO: write kmem_manager tests

// TODO: hihi remove
 void print_bitmap (); 
#endif // !__KMEM_MANAGER__
