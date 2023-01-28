#ifndef __KMEM_MANAGER__
#define __KMEM_MANAGER__

#include <libk/multiboot.h>
#include <libk/stddef.h>
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/PagingComplex.h"

// some faultcodes
#define PRESENT_VIOLATION       0x1
#define WRITE_VIOLATION         0x2
#define ACCESS_VIOLATION        0x4
#define RESERVED_VIOLATION      0x8
#define FETCH_VIOLATION         0x10


// Kernel high virtual base
#define HIGH_MAP_BASE           0xffffff8000000000UL 
// Physical range base
#define PHYSICAL_RANGE_BASE     0xffffff0000000000UL

// paging masks
#define PAGE_SIZE_MASK          0xFFFFffffFFFFf000UL
#define PAGE_LOW_MASK           0xFFFUL
#define ENTRY_MASK              0x1FFUL

#define PAGE_SIZE               0x200000
#define SMALL_PAGE_SIZE         0x1000UL
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
#define KMEM_FLAG_NOEXECUTE     0x20

// Custom mapping flags
#define KMEM_CUSTOMFLAG_GET_MAKE            0x01
#define KMEM_CUSTOMFLAG_CREATE_USER         0x02
#define KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE 0x04

// defines for alignment
#define ALIGN_UP(addr, size) \
    ((addr % size == 0) ? (addr) : (addr) + size - ((addr) % size))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % size))

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

void init_kmem_manager (uintptr_t* mb_addr, uintptr_t mb_first_addr, uintptr_t first_valid_alloc_addr);

void protect_kernel();
void protect_heap();

void prep_mmap (struct multiboot_tag_mmap* mmap);
void parse_memmap ();

uintptr_t kmem_get_page_idx (uintptr_t page_addr);
uintptr_t kmem_get_page_base (uintptr_t page_addr);
uintptr_t kmem_get_page_addr (uintptr_t page_idx);

void* kmem_from_phys (uintptr_t addr);
uintptr_t kmem_to_phys (PagingComplex_t* root, uintptr_t addr);

void kmem_set_phys_page_used (uintptr_t idx);
void kmem_set_phys_page_free (uintptr_t idx);
void kmem_set_phys_page (uintptr_t idx, bool value);
bool kmem_is_phys_page_used (uintptr_t idx);

void kmem_nuke_pd(uintptr_t vaddr);
void kmem_flush_tlb();

ErrorOrPtr kmem_request_pysical_page();
ErrorOrPtr kmem_prepare_new_physical_page();
PagingComplex_t* kmem_get_krnl_dir ();
PagingComplex_t* kmem_get_page(PagingComplex_t* root, uintptr_t addr, unsigned int kmem_flags);
PagingComplex_t* kmem_clone_page(PagingComplex_t* page);
void kmem_set_page_flags (PagingComplex_t* page, unsigned int flags);

/* mem mapping */

//void kmem_map_memory (uintptr_t vaddr, uintptr_t paddr, unsigned int flags);
bool kmem_map_page (PagingComplex_t* table, uintptr_t virt, uintptr_t phys, unsigned int flags, uint32_t page_flags);
// bool kmem_unmap_page (PagingComplex_t* table, uintptr_t virt); TODO
bool kmem_map_range (uintptr_t virt_base, uintptr_t phys_base, size_t page_count, unsigned int flags);

//void kmem_umap_memory (uintptr_t vaddr);
void kmem_init_physical_allocator();

void* kmem_kernel_alloc (uintptr_t addr, size_t size, int flags);
void* kmem_kernel_alloc_extended (uintptr_t addr, size_t size, int flags, int page_flags);

ErrorOrPtr kmem_kernel_alloc_range (size_t size, int custom_flags, int page_flags);

/* access to kmem_manager data struct */
const list_t* kmem_get_phys_ranges_list();

PagingComplex_t* kmem_create_user_page_map(size_t byte_size);

// TODO: write kmem_manager tests

// TODO: hihi remove
 void print_bitmap (); 
#endif // !__KMEM_MANAGER__
