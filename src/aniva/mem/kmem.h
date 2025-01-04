#ifndef __kmem__
#define __kmem__

#include "libk/flow/error.h"
#include "mem/page_dir.h"
#include "mem/pg.h"
#include <libk/multiboot.h>
#include <libk/stddef.h>
#include <libk/string.h>

struct proc;
struct page_tracker;

enum KMEM_MEMORY_TYPE {
    MEMTYPE_USABLE = 1,
    MEMTYPE_RESERVED,
    MEMTYPE_KERNEL_RESERVED,
    MEMTYPE_KERNEL,
    MEMTYPE_ACPI_RECLAIM,
    MEMTYPE_ACPI_NVS,
    MEMTYPE_BAD_MEM,
    MEMTYPE_UNKNOWN,
};

typedef struct {
    enum KMEM_MEMORY_TYPE type;
    paddr_t start;
    size_t length;
} kmem_range_t;

/*
 * Recieve information about the current state of a certain kmem
 * instance on a CPU
 */
typedef struct kmem_info {
    uint32_t cpu_id;
    uint32_t flags;
    uint64_t used_pages;
    size_t total_pages;

    /* TODO: keep track of DMA */
    uint32_t dma_buffer_count;
    /* TODO: virtual memory (on-disk memory) */
    uint32_t swap_page_count;
} kmem_info_t;

int kmem_get_info(kmem_info_t* info_buffer, uint32_t cpu_id);

void debug_kmem(void);

// some faultcodes
#define PRESENT_VIOLATION 0x1
#define WRITE_VIOLATION 0x2
#define ACCESS_VIOLATION 0x4
#define RESERVED_VIOLATION 0x8
#define FETCH_VIOLATION 0x10

#define MAX_VIRT_ADDR 0xffffffffffffffffULL
// Kernel text high virtual base
#define HIGH_MAP_BASE 0xFFFFFFFF80000000ULL
// Map base for any new kernel range allocs
#define KERNEL_MAP_BASE 0xFFFF800000000000ULL
// Anything IO can be mapped here
#define IO_MAP_BASE 0xffff000000000000ULL
// Base for early kernelheap mappings
#define EARLY_KERNEL_HEAP_BASE ALIGN_UP((uintptr_t)&_kernel_end, SMALL_PAGE_SIZE)
// Base for early multiboot fb
#define EARLY_FB_MAP_BASE 0xffffff8000000000ULL
// Base for the quickmap engine. We take the pretty much highest possible vaddr
#define QUICKMAP_BASE 0xFFFFffffFFFF0000ULL

/* Bottom of the userstack per process. Every thread gets thread_id * STACK_SIZE as a new base */
#define HIGH_STACK_BASE 0x0000000800000000ULL

/* We need to be carefull, because the userstack is placed directly under the kernel */
#define THREAD_ENTRY_BASE 0xFFFFFFFF00000000ULL

// paging masks
#define PDE_SIZE_MASK 0xFFFFffffFFFFF000ULL
#define PAGE_LOW_MASK 0xFFFULL
#define PTE_PAGE_MASK 0x8000000000000fffULL
#define ENTRY_MASK 0x1FFULL

#define PAGE_SIZE 0x200000
#define PAGE_SHIFT 12
#define LARGE_PAGE_SHIFT 21
#define SMALL_PAGE_SIZE (1 << PAGE_SHIFT)
#define LARGE_PAGE_SIZE (1 << LARGE_PAGE_SHIFT)
#define PAGE_SIZE_BYTES 0x200000UL

#define PDP_MASK 0x3fffffffUL
#define PD_MASK 0x1fffffUL
#define PT_MASK PAGE_LOW_MASK

// page flags
#define KMEM_FLAG_KERNEL 0x00000001
#define KMEM_FLAG_WRITABLE 0x00000002
#define KMEM_FLAG_NOCACHE 0x00000004
#define KMEM_FLAG_WRITETHROUGH 0x00000008
#define KMEM_FLAG_SPEC 0x00000010
#define KMEM_FLAG_NOEXECUTE 0x00000020
#define KMEM_FLAG_GLOBAL 0x00000040
#define KMEM_FLAG_EXPORTED 0x00000080
#define KMEM_FLAG_WC (KMEM_FLAG_NOCACHE | KMEM_FLAG_WRITETHROUGH | KMEM_FLAG_SPEC)
#define KMEM_FLAG_DMA (KMEM_FLAG_NOCACHE | KMEM_FLAG_WRITABLE)

// Custom mapping flags
#define KMEM_CUSTOMFLAG_GET_MAKE 0x00000001
#define KMEM_CUSTOMFLAG_CREATE_USER 0x00000002
#define KMEM_CUSTOMFLAG_IDENTITY 0x00000004
#define KMEM_CUSTOMFLAG_NO_REMAP 0x00000008
#define KMEM_CUSTOMFLAG_GIVE_PHYS 0x00000010
#define KMEM_CUSTOMFLAG_NO_PHYS_REALIGN 0x00000020
#define KMEM_CUSTOMFLAG_RECURSIVE_UNMAP 0x00000040
#define KMEM_CUSTOMFLAG_GET_PDE 0x00000080

#define KMEM_STATUS_FLAG_DONE_INIT 0x00000001
#define KMEM_STATUS_FLAG_HAS_QUICKMAP 0x00000002


// defines for alignment
#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

// #define GET_PAGECOUNT_EX(bytes, page_shift) (ALIGN_UP((bytes), (1 << (page_shift))) >> (page_shift))
// #define GET_PAGECOUNT(bytes) (ALIGN_UP((bytes), SMALL_PAGE_SIZE) >> 12)

#define GET_PAGECOUNT_EX(start, len, page_shift) ((ALIGN_UP((start) + (len), (1 << (page_shift))) - ALIGN_DOWN((start), (1 << (page_shift)))) >> (page_shift))
#define GET_PAGECOUNT(start, len) GET_PAGECOUNT_EX(start, len, PAGE_SHIFT)

static inline uintptr_t kmem_get_page_idx(uintptr_t page_addr)
{
    return (page_addr >> PAGE_SHIFT);
}

static inline uintptr_t kmem_get_page_addr(uintptr_t page_idx)
{
    return (page_idx << PAGE_SHIFT);
}

static inline uintptr_t kmem_get_page_base(uintptr_t base)
{
    return (base & ~(PTE_PAGE_MASK));
}

static inline uintptr_t kmem_set_page_base(pml_entry_t* entry, uintptr_t page_base)
{
    /* Clear the previous address */
    entry->raw_bits &= PTE_PAGE_MASK;
    /* Write the new address */
    entry->raw_bits |= kmem_get_page_base(page_base);
    return entry->raw_bits;
}

size_t kmem_get_early_map_size();

/*
 * initialize kernel pagetables and physical allocator
 */
error_t init_kmem(uintptr_t* mb_addr);

error_t kmem_set_addrspace(page_dir_t* dir);
error_t kmem_set_addrspace_ex(page_dir_t* dir, struct proc* proc);
error_t kmem_get_addrspace(page_dir_t* dir);
error_t kmem_get_kernel_addrspace(page_dir_t* dir);
pml_entry_t* kmem_get_kernel_root_pd();
paddr_t kmem_get_kernel_root_pd_phys();

uintptr_t kmem_get_page_idx(uintptr_t page_addr);
uintptr_t kmem_get_page_base(uintptr_t base);
uintptr_t kmem_set_page_base(pml_entry_t* entry, uintptr_t page_base);
uintptr_t kmem_get_page_addr(uintptr_t page_idx);

vaddr_t kmem_from_phys(uintptr_t addr, vaddr_t vbase);
/* Grab the IO mapping for a given physical address */
vaddr_t kmem_from_dma(paddr_t addr);
/*
 * translate a physical address to a virtual address in the
 * kernel text mapping
 */
vaddr_t kmem_ensure_high_mapping(uintptr_t addr);

/*
 * translate a virtual address to a physical address in
 * a pagetable given by the caller
 */
uintptr_t kmem_to_phys(pml_entry_t* root, uintptr_t addr);
uintptr_t kmem_to_phys_aligned(pml_entry_t* root, uintptr_t addr);

/*
 * flush tlb completely
 */
void kmem_refresh_tlb();

kerror_t kmem_get_page(pml_entry_t** bentry, pml_entry_t* root, uintptr_t addr, uint32_t kmem_flags, uint32_t page_flags);
void kmem_set_page_flags(pml_entry_t* page, uint32_t flags);

/* mem mapping */

/*
 * Generic mapping functions
 */
bool kmem_map_page(pml_entry_t* table, uintptr_t virt, uintptr_t phys, uint32_t kmem_flags, uint32_t page_flags);
bool kmem_map_range(pml_entry_t* table, uintptr_t virt_base, uintptr_t phys_base, size_t page_count, uint32_t kmem_flags, uint32_t page_flags);
int kmem_copy_kernel_mapping(pml_entry_t* new_table);
bool kmem_unmap_page(pml_entry_t* table, uintptr_t virt);
bool kmem_unmap_page_ex(pml_entry_t* table, uintptr_t virt, uint32_t custom_flags);
bool kmem_unmap_range(pml_entry_t* table, uintptr_t virt, size_t page_count);
bool kmem_unmap_range_ex(pml_entry_t* table, uintptr_t virt, size_t page_count, uint32_t custom_flags);

/*
 * Check if the process has access to the specified range
 */
int kmem_validate_ptr(struct proc* process, vaddr_t v_address, size_t size);
int kmem_ensure_mapped(pml_entry_t* table, vaddr_t base, size_t size);

int kmem_alloc_phys_page(paddr_t* p_addr);

/*
 * These functions are all about mapping and allocating
 * memory using the global physical memory bitmap
 */
int kmem_kernel_alloc(void** p_result, uintptr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags);
int kmem_kernel_alloc_range(void** p_result, size_t size, uint32_t custom_flags, uint32_t page_flags);

int kmem_dma_alloc(void** p_result, uintptr_t addr, size_t size, uint32_t custom_flag, uint32_t page_flags);
int kmem_dma_alloc_range(void** p_result, size_t size, uint32_t custom_flag, uint32_t page_flags);

int kmem_alloc(void** p_result, pml_entry_t* map, struct page_tracker* tracker, paddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags);
int kmem_alloc_ex(void** p_result, pml_entry_t* map, struct page_tracker* tracker, paddr_t addr, vaddr_t vbase, size_t size, uint32_t custom_flags, uintptr_t page_flags);

int kmem_alloc_range(void** p_result, pml_entry_t* map, struct page_tracker* tracker, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags);

int kmem_dealloc(pml_entry_t* map, struct page_tracker* tracker, uintptr_t virt_base, size_t size);
int kmem_dealloc_unmap(pml_entry_t* map, struct page_tracker* tracker, uintptr_t virt_base, size_t size);
int kmem_dealloc_ex(pml_entry_t* map, struct page_tracker* tracker, uintptr_t virt_base, size_t size, bool unmap, bool defer_res_release);

int kmem_kernel_dealloc(uintptr_t virt_base, size_t size);

int kmem_alloc_scattered(void** result, pml_entry_t* map, struct page_tracker* tracker, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags);

int kmem_user_alloc(void** p_result, struct proc* p, paddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags);
int kmem_user_alloc_range(void** p_result, struct proc* p, size_t size, uint32_t custom_flags, uint32_t page_flags);
int kmem_user_alloc_scattered(void** p_result, struct proc* p, vaddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags);
int kmem_user_dealloc(struct proc* p, vaddr_t vaddr, size_t size);

/*
 * Return a translation of the address provided that is
 * useable for the kernel
 *
 * Fails if virtual_address is not mapped into the provided map
 */
int kmem_get_kernel_address(vaddr_t* p_kaddr, vaddr_t virtual_address, pml_entry_t* from_map);
int kmem_get_kernel_address_ex(vaddr_t* p_kaddr, vaddr_t virtual_address, vaddr_t map_base, pml_entry_t* from_map);
int kmem_map_to_kernel(vaddr_t* p_kaddr, vaddr_t uaddr, size_t size, vaddr_t map_base, pml_entry_t* from_map, u32 custom_flags, u32 kmem_flags);

/*
 * Prepares a new pagemap that has virtual memory mapped from 0 -> initial_size
 * This can act as clean userspace directory creation, though a lot is still missing
 */
int kmem_create_page_dir(page_dir_t* dir, uint32_t custom_flags, size_t initial_size);
int kmem_copy_page_dir(page_dir_t* result, page_dir_t* src_dir);

/*
 * Free this entire addressspace for future use
 */
int kmem_destroy_page_dir(pml_entry_t* dir);

// TODO: write kmem tests

#endif // !__kmem__
