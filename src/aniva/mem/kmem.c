#include "kmem.h"
#include "entry/entry.h"
#include "irq/interrupts.h"
#include "libk/flow/error.h"
#include "libk/multiboot.h"
#include "logging/log.h"
#include "mem/page_dir.h"
#include "mem/pg.h"
#include "mem/tracker/tracker.h"
#include "phys.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <libk/stddef.h>
#include <libk/string.h>
#include <mem/heap.h>

/*
 * Aniva kernel memory manager
 *
 * This file is a big mess. There is unused code, long and unreadable code, stuff that's half broken and
 * stuff that just really likes to break for some reason. This file needs a big dust-off ASAP
 */

#define STANDARD_PD_ENTRIES 512

/*
 * Why is this a struct?
 *
 * Well, I wanted to have a more modulare kmem system, but it turned out to be
 * static as all hell. I've kept this so far since it might come in handy when I want
 * to migrate this code to support SMP eventualy. Then there should be multiple kmem managers
 * running on more than one CPU which makes it nice to have a struct per manager
 * to keep track of their state.
 */
static struct {
    uint32_t m_kmem_flags;

    // bitmap_t* m_phys_bitmap;

    /*
     * This is the base that we use for high mappings
     * Before we have done the late init this is HIGH_MAP_BASE
     * After we have done the late init this is KERNEL_MAP_BASE
     * Both of these virtual bases map to phys 0 which makes them
     * easy to work with in terms of transforming phisical to
     * virtual addresses, but the only difference is that KERNEL_MAP_BASE
     * has more range
     */
    vaddr_t m_high_page_base;

    page_dir_t kernel_pdir;

    //pml_entry_t* m_kernel_base_pd;
} KMEM_DATA;

/* Variable from boot.asm */
extern const size_t early_map_size;
static mutex_t* _kmem_map_lock;

size_t kmem_get_early_map_size()
{
    return early_map_size;
}

static inline u32 generate_tracker_flags(u32 custom_flags, u32 page_flags)
{
    u32 ret = 0;

    if (page_flags & KMEM_FLAG_KERNEL)
        ret |= PAGE_RANGE_FLAG_KERNEL; 

    if ((page_flags & KMEM_FLAG_NOEXECUTE) != KMEM_FLAG_NOEXECUTE)
        ret |= PAGE_RANGE_FLAG_EXEC;

    if (page_flags & KMEM_FLAG_WRITABLE)
        ret |= PAGE_RANGE_FLAG_WRITABLE;

    return ret;
}

void debug_kmem(void)
{
    uint32_t idx = 0;

    for (kmem_range_t* range = kmem_phys_get_range(idx); range; range = kmem_phys_get_range(idx)) {

        printf("Memory range (%d) starting at 0x%llx with 0x%llx bytes and type=%d\n", idx, range->start, range->length, range->type);

        idx++;
    }

    // printf("Got phys bitmap: 0x%llx (%lld bytes)\n", (uint64_t)KMEM_DATA.m_phys_bitmap & ~(HIGH_MAP_BASE), KMEM_DATA.m_phys_bitmap->m_size);
}

int kmem_get_info(kmem_info_t* info_buffer, uint32_t cpu_id)
{
    if (!info_buffer)
        return -1;

    memset(info_buffer, 0, sizeof(*info_buffer));

    info_buffer->cpu_id = cpu_id;
    info_buffer->flags = KMEM_DATA.m_kmem_flags;
    info_buffer->total_pages = kmem_phys_get_total_bytecount() >> PAGE_SHIFT;
    info_buffer->used_pages = kmem_phys_get_used_bytecount() >> PAGE_SHIFT;

    //KLOG_ERR("Got info buffer (used: %lld, total: %lld)\n", info_buffer->used_pages, info_buffer->total_pages);

    //kmem_phys_dump();

    return 0;
}

/*
 * Transform a physical address to an arbitrary virtual base
 */
vaddr_t kmem_from_phys(uintptr_t addr, vaddr_t vbase)
{
    return (vaddr_t)(vbase | addr);
}

vaddr_t kmem_from_dma(paddr_t addr)
{
    return kmem_from_phys(addr, IO_MAP_BASE);
}

/*
 * Transform a physical address to an address that is mapped to the
 * high kernel range
 */
vaddr_t kmem_ensure_high_mapping(uintptr_t addr)
{
    return kmem_from_phys(addr, HIGH_MAP_BASE);
}

/*
 * Translate a virtual address to a physical address that is
 * page-aligned
 *
 * TODO: change this function so errors are communicated clearly
 */
uintptr_t kmem_to_phys_aligned(pml_entry_t* root, vaddr_t addr)
{
    pml_entry_t* page;

    if (kmem_get_page(&page, root, ALIGN_DOWN(addr, SMALL_PAGE_SIZE), 0, 0))
        return NULL;

    return kmem_get_page_base(page->raw_bits);
}

/*!
 * @brief: Get a physical address by mapping
 *
 * Same as the function above, but this one also keeps the alignment in mind
 */
uintptr_t kmem_to_phys(pml_entry_t* root, vaddr_t addr)
{
    size_t delta;
    pml_entry_t* page;

    /* Address is not mapped */
    if (kmem_get_page(&page, root, addr, 0, 0))
        return NULL;

    delta = addr - ALIGN_DOWN(addr, SMALL_PAGE_SIZE);

    return kmem_get_page_base(page->raw_bits) + delta;
}

/*
 * Try to find a free physical page, just as kmem_phys_get_page,
 * but this also marks it as used and makes sure it is filled with zeros
 *
 * FIXME: we might grab a page that is located at kernel_end + 1 Gib which won't be
 * mapped to our kernel HIGH map extention. When this happens, we want to have
 */
int kmem_alloc_phys_page(paddr_t* p_addr)
{
    int error;
    paddr_t address;
    paddr_t p_page_idx;

    // Find the thing
    error = kmem_phys_alloc_page(&p_page_idx);

    if (error)
        return error;

    address = kmem_get_page_addr(p_page_idx);

    // There might be an issue here as we try to zero
    // and this page is never mapped. We might need some
    // sort of temporary mapping like serenity has to do
    // this, but we might also rewrite this entire thing
    // as to map this sucker before we zero. This would
    // mean that you can't just get a nice clean page...
    memset((void*)kmem_from_phys(address, KMEM_DATA.m_high_page_base), 0x00, SMALL_PAGE_SIZE);

    if (p_addr)
        *p_addr = address;

    return 0;
}

pml_entry_t* kmem_get_kernel_root_pd()
{
    return KMEM_DATA.kernel_pdir.m_root;
}

paddr_t kmem_get_kernel_root_pd_phys()
{
    return KMEM_DATA.kernel_pdir.m_phys_root;
}

/*
 * Find the kernel root page directory
 */
error_t kmem_get_kernel_addrspace(page_dir_t* dir)
{
    if (!dir)
        return -EINVAL;

    /* Copy our our glorious kernel page directory */
    memcpy(dir, &KMEM_DATA.kernel_pdir, sizeof(*dir));

    return 0;
}

error_t kmem_get_addrspace(page_dir_t* dir)
{
    /* Check if we're in a processes addresspace */
    proc_t* c_proc = get_current_proc();

    /* No process, just return the kernel addresspace */
    if (!c_proc)
        return kmem_get_kernel_addrspace(dir);

    /* Copy out the directory */
    memcpy(dir, &c_proc->m_root_pd, sizeof(*dir));

    return 0;
}

static inline kerror_t _allocate_pde(pml_entry_t* pde, uint32_t custom_flags, uint32_t page_flags)
{
    kerror_t error;
    const bool pd_entry_exists = pml_entry_is_bit_set(pde, PDE_PRESENT);

    /* Not allowed to make it, but no dice =( */
    if (!pd_entry_exists && ((custom_flags & KMEM_CUSTOMFLAG_GET_MAKE) != KMEM_CUSTOMFLAG_GET_MAKE))
        return -KERR_NOMEM;

    if (!pd_entry_exists) {
        vaddr_t page_addr;

        error = kmem_alloc_phys_page(&page_addr);

        /* Mega fuck */
        if (error)
            return error;

        kmem_set_page_base(pde, page_addr);
        pml_entry_set_bit(pde, PDE_PRESENT, true);
        pml_entry_set_bit(pde, PDE_WRITABLE, (page_flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE);
        pml_entry_set_bit(pde, PDE_GLOBAL, (page_flags & KMEM_FLAG_GLOBAL) == KMEM_FLAG_GLOBAL);
        pml_entry_set_bit(pde, PDE_NX, (page_flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE);
        pml_entry_set_bit(pde, PDE_WRITE_THROUGH, (page_flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH);
        pml_entry_set_bit(pde, PDE_NO_CACHE, (page_flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE);

        pml_entry_set_bit(pde, PDE_USER,
            (custom_flags & KMEM_CUSTOMFLAG_CREATE_USER) == KMEM_CUSTOMFLAG_CREATE_USER ? true : (page_flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL);
    }

    return 0;
}

/*
 * This function is to be used after the bootstrap pagemap has been
 * discarded. When we create a new mapping, we pull a page from a
 * pool of virtual memory that is only used for that purpose and sustains itself
 * by mapping the pages it uses for the creation of the pagetable into itself.
 *
 * TODO: implement this and also create easy virtual pool allocation, creation, deletion,
 * ect. I want to be able to easily see which virtual memory is used per pagemap and
 * we need a good idea of where we map certain resources and things (i.e. drivers, I/O ranges,
 * generic kernel_resources, ect.)
 */
kerror_t kmem_get_page(pml_entry_t** bentry, pml_entry_t* root, uintptr_t addr, uint32_t kmem_flags, uint32_t page_flags)
{
    kerror_t error;
    pml_entry_t* pml4;
    pml_entry_t* pdp;
    pml_entry_t* pd;
    pml_entry_t* pt;

    /* Make sure the virtual address is not fucking us in our ass */
    addr = addr & PDE_SIZE_MASK;

    /* Compute indices */
    const uint16_t pml4_idx = (addr >> 39) & ENTRY_MASK;
    const uint16_t pdp_idx = (addr >> 30) & ENTRY_MASK;
    const uint16_t pd_idx = (addr >> 21) & ENTRY_MASK;
    const uint16_t pt_idx = (addr >> 12) & ENTRY_MASK;

    pml4 = (root == nullptr) ? (pml_entry_t*)kmem_from_phys((uintptr_t)KMEM_DATA.kernel_pdir.m_phys_root, KMEM_DATA.m_high_page_base) : root;
    error = _allocate_pde(&pml4[pml4_idx], kmem_flags, page_flags);

    if (error)
        return error;

    pdp = (pml_entry_t*)kmem_from_phys((uintptr_t)kmem_get_page_base(pml4[pml4_idx].raw_bits), KMEM_DATA.m_high_page_base);
    error = _allocate_pde(&pdp[pdp_idx], kmem_flags, page_flags);

    if (error)
        return error;

    pd = (pml_entry_t*)kmem_from_phys((uintptr_t)kmem_get_page_base(pdp[pdp_idx].raw_bits), KMEM_DATA.m_high_page_base);
    error = _allocate_pde(&pd[pd_idx], kmem_flags, page_flags);

    if (error)
        return error;

    // this just should exist
    pt = (pml_entry_t*)kmem_from_phys((uintptr_t)kmem_get_page_base(pd[pd_idx].raw_bits), KMEM_DATA.m_high_page_base);

    if (bentry)
        *bentry = (pml_entry_t*)&pt[pt_idx];

    return KERR_NONE;
}

void kmem_invalidate_tlb_cache_entry(uintptr_t vaddr)
{
    asm volatile("invlpg (%0)" : : "r"(vaddr));

    /* TODO: send message to other cores that they need to
    perform a tlb shootdown aswell on this address */
}

void kmem_invalidate_tlb_cache_range(uintptr_t vaddr, size_t size)
{
    vaddr_t virtual_base = ALIGN_DOWN(vaddr, SMALL_PAGE_SIZE);
    uintptr_t indices = kmem_get_page_idx(size);

    for (uintptr_t i = 0; i < indices; i++) {
        kmem_invalidate_tlb_cache_entry(virtual_base + i * SMALL_PAGE_SIZE);
    }
}

void kmem_refresh_tlb(void)
{
    /* FIXME: is this always up to date? */
    page_dir_t* current = get_current_processor()->m_page_dir;

    /* Reloading CR3 will clear the tlb */
    kmem_set_addrspace(current);
}

bool kmem_map_page(pml_entry_t* table, vaddr_t virt, paddr_t phys, uint32_t kmem_flags, uint32_t page_flags)
{
    return kmem_map_range(table, virt, phys, 1, kmem_flags, page_flags);
}

bool kmem_map_range(pml_entry_t* table, uintptr_t virt_base, uintptr_t phys_base, size_t page_count, uint32_t kmem_flags, uint32_t page_flags)
{
    pml_entry_t* page;

    /* Clear the low page bits from these addresses */
    virt_base &= ~PAGE_LOW_MASK;
    phys_base &= ~PAGE_LOW_MASK;

    /*
    if (!page_flags)
      kernel_panic("Mapped page with page_flags=NULL");
    */

    mutex_lock(_kmem_map_lock);

    for (uintptr_t i = 0; i < page_count; i++) {
        if (kmem_get_page(&page, table, virt_base + (i << PAGE_SHIFT), kmem_flags, page_flags)) {
            mutex_unlock(_kmem_map_lock);
            return false;
        }

        /* Do the actual mapping */
        kmem_set_page_base(page, phys_base + (i << PAGE_SHIFT));

        /* Set regular bits */
        pml_entry_set_bit(page, PTE_PRESENT, true);
        pml_entry_set_bit(page, PTE_WRITABLE, ((page_flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE));
        pml_entry_set_bit(page, PTE_NO_CACHE, ((page_flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE));
        pml_entry_set_bit(page, PTE_WRITE_THROUGH, ((page_flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH));
        pml_entry_set_bit(page, PTE_PAT, ((page_flags & KMEM_FLAG_SPEC) == KMEM_FLAG_SPEC));
        pml_entry_set_bit(page, PTE_NX, ((page_flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE));

        /* Set special bit */
        pml_entry_set_bit(page, PTE_USER,
            (kmem_flags & KMEM_CUSTOMFLAG_CREATE_USER) == KMEM_CUSTOMFLAG_CREATE_USER ? true : (page_flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL);
    }

    mutex_unlock(_kmem_map_lock);
    return true;
}

/* Assumes we have a physical address buffer */
#define DO_PD_CHECK(parent, entry, idx)                         \
    do {                                                        \
        for (uintptr_t i = 0; i < 512; i++) {                   \
            if (pml_entry_is_bit_set(&entry[i], PTE_PRESENT)) { \
                return false;                                   \
            }                                                   \
        }                                                       \
        parent[idx].raw_bits = NULL;                            \
        phys = kmem_to_phys_aligned(table, (vaddr_t)entry);     \
        kmem_phys_dealloc_page(kmem_get_page_idx(phys));        \
    } while (0)

static bool __kmem_do_recursive_unmap(pml_entry_t* table, uintptr_t virt)
{
    /* This part is a small copy of kmem_get_page, but we need our own nuance here, so fuck off will ya? */
    bool entry_exists;
    paddr_t phys;

    /* Indices we need for recursive deallocating */
    const uintptr_t pml4_idx = (virt >> 39) & ENTRY_MASK;
    const uintptr_t pdp_idx = (virt >> 30) & ENTRY_MASK;
    const uintptr_t pd_idx = (virt >> 21) & ENTRY_MASK;

    pml_entry_t* pml4 = (table == nullptr) ? (pml_entry_t*)kmem_ensure_high_mapping(kmem_get_kernel_root_pd_phys()) : table;
    entry_exists = (pml_entry_is_bit_set(&pml4[pml4_idx], PDE_PRESENT));

    if (!entry_exists)
        return false;

    pml_entry_t* pdp = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pml4[pml4_idx].raw_bits));
    entry_exists = (pml_entry_is_bit_set(&pdp[pdp_idx], PDE_PRESENT));

    if (!entry_exists)
        return false;

    pml_entry_t* pd = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pdp[pdp_idx].raw_bits));
    entry_exists = (pml_entry_is_bit_set(&pd[pd_idx], PDE_PRESENT));

    if (!entry_exists)
        return false;

    pml_entry_t* pt = (pml_entry_t*)kmem_ensure_high_mapping((uintptr_t)kmem_get_page_base(pd[pd_idx].raw_bits));

    DO_PD_CHECK(pd, pt, pd_idx);
    DO_PD_CHECK(pdp, pd, pdp_idx);
    DO_PD_CHECK(pml4, pdp, pml4_idx);

    return true;
}

bool kmem_unmap_page_ex(pml_entry_t* table, uintptr_t virt, uint32_t custom_flags)
{
    pml_entry_t* page;

    if (kmem_get_page(&page, table, virt, custom_flags, 0))
        return false;

    page->raw_bits = NULL;

    if ((custom_flags & KMEM_CUSTOMFLAG_RECURSIVE_UNMAP) == KMEM_CUSTOMFLAG_RECURSIVE_UNMAP)
        __kmem_do_recursive_unmap(table, virt);

    kmem_invalidate_tlb_cache_entry(virt);

    return true;
}

// FIXME: free higher level pts as well
bool kmem_unmap_page(pml_entry_t* table, uintptr_t virt)
{
    return kmem_unmap_page_ex(table, virt, 0);
}

bool kmem_unmap_range(pml_entry_t* table, uintptr_t virt, size_t page_count)
{
    return kmem_unmap_range_ex(table, virt, page_count, NULL);
}

bool kmem_unmap_range_ex(pml_entry_t* table, uintptr_t virt, size_t page_count, uint32_t custom_flags)
{
    virt = ALIGN_DOWN(virt, SMALL_PAGE_SIZE);

    for (uintptr_t i = 0; i < page_count; i++) {

        if (!kmem_unmap_page_ex(table, virt, custom_flags))
            return false;

        virt += SMALL_PAGE_SIZE;
    }

    return true;
}

/*
 * NOTE: this function sets the flags for a page entry, not
 * any other entries like page directory entries or any of the such.
 *
 * NOTE: this also always sets the PRESENT bit to true
 */
void kmem_set_page_flags(pml_entry_t* page, uint32_t flags)
{
    /* Make sure PAT is enabled if we want different caching options */
    if ((flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE || (flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH)
        flags |= KMEM_FLAG_SPEC;

    pml_entry_set_bit(page, PTE_PRESENT, true);
    pml_entry_set_bit(page, PTE_USER, ((flags & KMEM_FLAG_KERNEL) != KMEM_FLAG_KERNEL));
    pml_entry_set_bit(page, PTE_WRITABLE, ((flags & KMEM_FLAG_WRITABLE) == KMEM_FLAG_WRITABLE));
    pml_entry_set_bit(page, PTE_NO_CACHE, ((flags & KMEM_FLAG_NOCACHE) == KMEM_FLAG_NOCACHE));
    pml_entry_set_bit(page, PTE_WRITE_THROUGH, ((flags & KMEM_FLAG_WRITETHROUGH) == KMEM_FLAG_WRITETHROUGH));
    pml_entry_set_bit(page, PTE_PAT, ((flags & KMEM_FLAG_SPEC) == KMEM_FLAG_SPEC));
    pml_entry_set_bit(page, PTE_NX, ((flags & KMEM_FLAG_NOEXECUTE) == KMEM_FLAG_NOEXECUTE));
}

/*!
 * @brief Check if the specified process has access to an address
 *
 * when process is null, we simply use the kernel process
 */
int kmem_validate_ptr(struct proc* process, vaddr_t v_address, size_t size)
{
    vaddr_t v_offset;
    size_t p_page_count;
    pml_entry_t* root;
    pml_entry_t* page;

    root = (process ? (process->m_root_pd.m_root) : nullptr);

    p_page_count = ALIGN_UP(size, SMALL_PAGE_SIZE) / SMALL_PAGE_SIZE;

    for (uint64_t i = 0; i < p_page_count; i++) {
        v_offset = v_address + (i * SMALL_PAGE_SIZE);

        if (v_offset >= HIGH_MAP_BASE)
            return -1;

        if (kmem_get_page(&page, root, v_offset, NULL, NULL))
            return -1;

        /* Should be mapped AND should be mapped as user */
        if (!pml_entry_is_bit_set(page, PTE_USER | PTE_PRESENT))
            return -1;

        /* TODO: check if the physical address is within a reserved range */
        /* TODO: check if the process owns the virtual range in its resource map */
        // p_addr = kmem_to_phys(root, v_offset);
    }

    return 0;
}

int kmem_ensure_mapped(pml_entry_t* table, vaddr_t base, size_t size)
{
    pml_entry_t* page;
    vaddr_t vaddr;
    size_t page_count;

    page_count = GET_PAGECOUNT(base, size);

    for (uint64_t i = 0; i < page_count; i++) {

        vaddr = base + (i * SMALL_PAGE_SIZE);

        if (!KERR_OK(kmem_get_page(&page, table, vaddr, NULL, NULL)))
            return -1;
    }

    return 0;
}

/*
 * NOTE: About the 'current_driver' mechanism
 *
 * We track allocations of drivers by keeping track of which driver we are currently executing, but there is nothing stopping the
 * drivers (yet) from using any kernel function that bypasses this mechanism, so mallicious (or simply malfunctioning) drivers can
 * cause us to lose memory at a big pase. We should (TODO) limit drivers to only use the functions that are deemed safe to use by drivers,
 * AKA we should blacklist any 'unsafe' functions, like kmem_alloc_ex, which bypasses any resource tracking
 */

int kmem_kernel_dealloc(uintptr_t virt_base, size_t size)
{
    return kmem_dealloc(nullptr, nullptr, virt_base, size);
}

int kmem_dealloc(pml_entry_t* map, page_tracker_t* tracker, uintptr_t virt_base, size_t size)
{
    /* NOTE: process is nullable, so it does not matter if we are not in a process at this point */
    return kmem_dealloc_ex(map, tracker, virt_base, size, true, false);
}

int kmem_dealloc_unmap(pml_entry_t* map, page_tracker_t* tracker, uintptr_t virt_base, size_t size)
{
    return kmem_dealloc_ex(map, tracker, virt_base, size, true, false);
}

/*!
 * @brief Expanded deallocation function
 *
 */
int kmem_dealloc_ex(pml_entry_t* map, page_tracker_t* tracker, uintptr_t virt_base, size_t size, bool unmap, bool defer_res_release)
{
    page_range_t range;
    const size_t pages_needed = GET_PAGECOUNT(virt_base, size);

    for (uintptr_t i = 0; i < pages_needed; i++) {
        // get the virtual address of the current page
        const vaddr_t vaddr = virt_base + (i * SMALL_PAGE_SIZE);
        // get the physical base of that page
        const paddr_t paddr = kmem_to_phys_aligned(map, vaddr);
        // get the index of that physical page
        const uintptr_t page_idx = kmem_get_page_idx(paddr);

        if (kmem_phys_dealloc_page(page_idx))
            return 2;

        if (unmap && !kmem_unmap_page(map, vaddr))
            return 1;
    }

    /* Only release the resource if we dont want to defer that opperation */
    //if (resources && !defer_res_release && resource_release(virt_base, size, GET_RESOURCE(resources, KRES_TYPE_MEM)))
        //return 1;

    /* We don't want to track this deallocation. Dip */
    if (!tracker || defer_res_release)
        return 0;

    /* Initialize the range */
    init_page_range(&range, kmem_get_page_idx(virt_base), pages_needed, NULL, 1);

    /* Deallocate it from the tracker */
    return page_tracker_dealloc(tracker, &range);
}

/*!
 * @brief Allocate a physical address for the kernel
 *
 */
int kmem_kernel_alloc(void** result, uintptr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    return kmem_alloc(result, nullptr, nullptr, addr, size, custom_flags, page_flags);
}

/*!
 * @brief Allocate a range of memory for the kernel
 *
 */
int kmem_kernel_alloc_range(void** result, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    return kmem_alloc_range(result, nullptr, nullptr, KERNEL_MAP_BASE, size, custom_flags, page_flags);
}

int kmem_dma_alloc(void** result, uintptr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    return kmem_alloc_ex(result, nullptr, nullptr, addr, KERNEL_MAP_BASE, size, custom_flags, page_flags | KMEM_FLAG_DMA);
}

int kmem_dma_alloc_range(void** result, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    return kmem_alloc_range(result, nullptr, nullptr, KERNEL_MAP_BASE, size, custom_flags, page_flags | KMEM_FLAG_DMA);
}

int kmem_alloc(void** result, pml_entry_t* map, page_tracker_t* tracker, paddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    return kmem_alloc_ex(result, map, tracker, addr, KERNEL_MAP_BASE, size, custom_flags, page_flags);
}

int kmem_alloc_ex(void** result, pml_entry_t* map, page_tracker_t* tracker, paddr_t addr, vaddr_t vbase, size_t size, uint32_t custom_flags, uintptr_t page_flags)
{
    const bool should_identity_map = (custom_flags & KMEM_CUSTOMFLAG_IDENTITY) == KMEM_CUSTOMFLAG_IDENTITY;
    const bool should_remap = (custom_flags & KMEM_CUSTOMFLAG_NO_REMAP) != KMEM_CUSTOMFLAG_NO_REMAP;

    const paddr_t phys_base = ALIGN_DOWN(addr, SMALL_PAGE_SIZE);
    const size_t pages_needed = GET_PAGECOUNT(addr, size);

    const vaddr_t virt_base = should_identity_map ? phys_base : (should_remap ? kmem_from_phys(phys_base, vbase) : vbase);

    /* Compute return value since we align the parameter */
    const vaddr_t ret = should_identity_map ? addr : (should_remap ? kmem_from_phys(addr, vbase) : vbase);

    /*
     * Mark our pages as used BEFORE we map the range, since map_page
     * sometimes yoinks pages for itself
     */
    kmem_phys_reserve_range(kmem_get_page_idx(phys_base), pages_needed);

    /* Don't mark, since we have already done that */
    if (!kmem_map_range(map, virt_base, phys_base, pages_needed, KMEM_CUSTOMFLAG_GET_MAKE | custom_flags, page_flags))
        return -1;

    if (tracker) {
        /*
         * NOTE: make sure to claim the range that we actually plan to use here, not what is actually
         * allocated internally. This is because otherwise we won't be able to find this resource again if we
         * try to release it
         */
        //resource_claim_ex("kmem alloc", nullptr, ALIGN_DOWN(ret, SMALL_PAGE_SIZE), pages_needed * SMALL_PAGE_SIZE, KRES_TYPE_MEM, resources);
        page_tracker_alloc(tracker, kmem_get_page_idx(ret), pages_needed, generate_tracker_flags(custom_flags, page_flags));
    }

    if (result)
        *result = (void*)(ret);
    return 0;
}

int kmem_alloc_range(void** result, pml_entry_t* map, page_tracker_t* tracker, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    int error;
    uintptr_t phys_idx;
    const size_t pages_needed = GET_PAGECOUNT(vbase, size);
    const bool should_identity_map = (custom_flags & KMEM_CUSTOMFLAG_IDENTITY) == KMEM_CUSTOMFLAG_IDENTITY;
    const bool should_remap = (custom_flags & KMEM_CUSTOMFLAG_NO_REMAP) != KMEM_CUSTOMFLAG_NO_REMAP;

    /*
     * Mark our pages as used BEFORE we map the range, since map_page
     * sometimes yoinks pages for itself
     */
    error = kmem_phys_alloc_range(pages_needed, &phys_idx);

    if (error)
        return error;

    /* Calculate the page addresses of this index */
    const paddr_t phys_base = kmem_get_page_addr(phys_idx);
    const vaddr_t virt_base = should_identity_map ? phys_base : (should_remap ? kmem_from_phys(phys_base, vbase) : vbase);

    if (!kmem_map_range(map, virt_base, phys_base, pages_needed, KMEM_CUSTOMFLAG_GET_MAKE | custom_flags, page_flags))
        return -1;

    if (tracker)
        //resource_claim_ex("kmem alloc range", nullptr, ALIGN_DOWN(virt_base, SMALL_PAGE_SIZE), (pages_needed * SMALL_PAGE_SIZE), KRES_TYPE_MEM, resources);
        page_tracker_alloc(tracker, kmem_get_page_idx(virt_base), pages_needed, generate_tracker_flags(custom_flags, page_flags));

    *result = (void*)virt_base;
    return 0;
}

/*
 * This function will never remap or use identity mapping, so
 * KMEM_CUSTOMFLAG_NO_REMAP and KMEM_CUSTOMFLAG_IDENTITY are ignored here
 */
int kmem_alloc_scattered(void** result, pml_entry_t* map, page_tracker_t* tracker, vaddr_t vbase, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    int error;
    paddr_t p_addr;
    vaddr_t v_addr;

    const vaddr_t v_base = ALIGN_DOWN(vbase, SMALL_PAGE_SIZE);
    const size_t pages_needed = GET_PAGECOUNT(vbase, size);

    /*
     * 1) Loop for as many times as we need pages
     * 1.1)  - Find a physical page to map
     * 1.2)  - Map the physical page to its corresponding virtual address
     * 2) Claim the resource (virtual addressrange)
     */
    for (uint64_t i = 0; i < pages_needed; i++) {

        error = kmem_alloc_phys_page(&p_addr);

        if (error)
            return error;

        v_addr = v_base + (i * SMALL_PAGE_SIZE);

        /*
         * NOTE: don't mark, because otherwise the physical pages gets
         * marked free again after it is mapped
         */
        if (!kmem_map_page(
                map,
                v_addr,
                p_addr,
                KMEM_CUSTOMFLAG_GET_MAKE | custom_flags,
                page_flags)) {
            return -1;
        }
    }

    /* Register this range in the page tracker, if it's specified */
    if (tracker)
        page_tracker_alloc(tracker, kmem_get_page_idx(v_base), pages_needed, generate_tracker_flags(custom_flags, page_flags));

    *result = (void*)vbase;
    return 0;
}

/*!
 * @brief: Allocate a scattered bit of memory for a userprocess
 *
 */
int kmem_user_alloc_range(void** result, struct proc* p, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    int error;
    u32 tracker_flags;
    page_range_t range;
    size_t nr_pages;

    if (!p || !size)
        return -1;

    nr_pages = GET_PAGECOUNT(0, size);
    tracker_flags = generate_tracker_flags(custom_flags, page_flags);

    /* FIXME: this is not very safe, we need to randomize the start of process data probably lmaoo */
    //error = resource_find_usable_range(p->m_resource_bundle, KRES_TYPE_MEM, size, &first_usable_base);
    error = page_tracker_alloc_any(&p->m_virtual_tracker, PAGE_TRACKER_FIRST_FIT, nr_pages, tracker_flags, &range);

    if (error)
        return error;

    /* TODO: Must calls in syscalls that fail may kill the process with the internal error flags set */
    return kmem_alloc_scattered(
        result,
        p->m_root_pd.m_root,
        nullptr,
        kmem_get_page_addr(range.page_idx),
        size,
        custom_flags | KMEM_CUSTOMFLAG_CREATE_USER,
        page_flags);
}

/*!
 * @brief: Deallocate userpages
 */
int kmem_user_dealloc(struct proc* p, vaddr_t vaddr, size_t size)
{
    paddr_t c_phys_base;
    uintptr_t c_phys_idx;
    page_range_t range;
    const size_t page_count = GET_PAGECOUNT(vaddr, size);

    for (uintptr_t i = 0; i < page_count; i++) {
        /* Grab the aligned physical base of this virtual address */
        c_phys_base = kmem_to_phys_aligned(p->m_root_pd.m_root, vaddr);

        /* Grab it's physical page index */
        c_phys_idx = kmem_get_page_idx(c_phys_base);

        /* Free the physical page */
        kmem_phys_dealloc_page(c_phys_idx);

        /* Unmap from the process */
        kmem_unmap_page(p->m_root_pd.m_root, vaddr);

        /* Next page */
        vaddr += SMALL_PAGE_SIZE;
    }

    /* Initialize a page range */
    init_page_range(&range, kmem_get_page_idx(vaddr), page_count, 0, 1);

    /* Deallocate the user range */
    page_tracker_dealloc(&p->m_virtual_tracker, &range);

    return (0);
}

/*!
 * @brief: Allocate a block of memory into a userprocess
 *
 */
int kmem_user_alloc(void** result, struct proc* p, paddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    int error;
    u32 tracker_flags;
    page_range_t range;
    size_t nr_pages;

    if (!p || !size)
        return -1;

    nr_pages = GET_PAGECOUNT(addr, size);
    tracker_flags = generate_tracker_flags(custom_flags, page_flags);

    /* FIXME: this is not very safe, we need to randomize the start of process data probably lmaoo */
    //error = resource_find_usable_range(&p->m_virtual_tracker, KRES_TYPE_MEM, size, &first_usable_base);
    error = page_tracker_alloc_any(&p->m_virtual_tracker, PAGE_TRACKER_FIRST_FIT, nr_pages, tracker_flags, &range);

    if (error)
        return error;

    /* NOTE: We don't supply a tracker, since we already allocated in the processes tracker */
    return kmem_alloc_ex(
        result,
        p->m_root_pd.m_root,
        nullptr,
        addr,
        kmem_get_page_addr(range.page_idx),
        size,
        custom_flags | KMEM_CUSTOMFLAG_CREATE_USER,
        page_flags);
}

int kmem_user_alloc_scattered(void** p_result, struct proc* p, vaddr_t addr, size_t size, uint32_t custom_flags, uint32_t page_flags)
{
    return kmem_alloc_scattered(p_result, p->m_root_pd.m_root, &p->m_virtual_tracker, addr, size, custom_flags | KMEM_CUSTOMFLAG_CREATE_USER, page_flags);
}

/*!
 * @brief Fixup the mapping from boot.asm
 *
 * boot.asm maps 1 Gib high, starting from address NULL, so we'll do the same
 * here to make sure the entire system stays happy after the pagemap switch
 */
static void __kmem_map_kernel_range_to_map(pml_entry_t* map)
{
    pml_entry_t* page;
    const size_t max_end_idx = kmem_get_page_idx(2ULL * Gib);

    for (uintptr_t i = 0; i < max_end_idx; i++) {
        if (kmem_get_page(&page, map, (i << PAGE_SHIFT) | HIGH_MAP_BASE, KMEM_CUSTOMFLAG_GET_MAKE, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE))
            break;

        kmem_set_page_base(page, (i << PAGE_SHIFT));
        kmem_set_page_flags(page, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE);
    }

    KLOG_INFO("Mapped kernel text\n");
}

static void __kmem_free_old_pagemaps()
{
    paddr_t p_base;
    size_t page_count;

    struct {
        vaddr_t base;
        size_t entry_count;
    } map_entries[] = {
        { (vaddr_t)boot_pml4t, 512 },
        { (vaddr_t)boot_pdpt, 512 },
        { (vaddr_t)boot_hh_pdpt, 512 },
        { (vaddr_t)boot_pd0, 512 },
        { (vaddr_t)boot_pd0_p, 0x40000 },
    };

    for (uintptr_t i = 0; i < arrlen(map_entries); i++) {
        p_base = map_entries[i].base & ~HIGH_MAP_BASE;
        page_count = map_entries[i].entry_count >> 9;

        KLOG_DBG("Setting old pagemap (vbase: 0x%llx, pbase: 0x%llx, pagecount: %lld) free\n",
            map_entries[i].base,
            p_base,
            page_count);

        kmem_phys_dealloc_range(
            kmem_get_page_idx(p_base),
            page_count);
    }
}

/*!
 * @brief: Create a new pagetable layout for the kernel and destroy the old one
 */
static void _init_kmem_page_layout(void)
{
    uintptr_t map;

    /* Grab the page for our pagemap root */
    ASSERT_MSG(kmem_alloc_phys_page(&map) == 0, "Failed to get kmem kernel root page");

    /* Set the page dir */
    KMEM_DATA.kernel_pdir = (page_dir_t) {
        .m_root = (pml_entry_t*)kmem_from_phys(map, HIGH_MAP_BASE),
        .m_phys_root = (u64)map,
    };
    KMEM_DATA.m_high_page_base = HIGH_MAP_BASE;

    /* NOTE: boot.asm also mapped identity */
    memset((void*)map, 0x00, SMALL_PAGE_SIZE);

    /* Assert that we got valid shit =) */
    ASSERT_MSG(kmem_get_kernel_root_pd_phys(), "Tried to init kmem_page_layout without a present krnl dir");

    /* Mimic boot.asm */
    __kmem_map_kernel_range_to_map((pml_entry_t*)map);

    /* Ensure interrupts are disabled */
    disable_interrupts();

    /* Load the new pagemap baby */
    kmem_set_addrspace(&KMEM_DATA.kernel_pdir);

    /* Return the old pagemap to the physical pool */
    __kmem_free_old_pagemaps();

    // kernel_panic("We reached hell =D");
}

/*!
 * @brief Create a new empty page directory that shares the kernel mappings
 *
 * Nothing to add here...
 */
int kmem_create_page_dir(page_dir_t* ret, uint32_t custom_flags, size_t initial_mapping_size)
{
    if (!ret)
        return -1;

    int error;

    /*
     * We can't just take a clean page here, since we need it mapped somewhere...
     * For that we'll have to use the kernel allocation feature to instantly map
     * it somewhere for us in kernelspace =D
     */
    pml_entry_t* table_root;

    error = (kmem_kernel_alloc_range((void**)&table_root, SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_CREATE_USER, KMEM_FLAG_WRITABLE));

    if (error)
        return error;

    /* Clear root, so we have no random mappings */
    memset(table_root, 0, SMALL_PAGE_SIZE);

    /* NOTE: this works, but I really don't want to have to do this =/ */
    error = (kmem_copy_kernel_mapping(table_root));

    if (error) {
        kmem_kernel_dealloc((vaddr_t)table_root, SMALL_PAGE_SIZE);
        return error;
    }

    const vaddr_t kernel_start = ALIGN_DOWN((uintptr_t)&_kernel_start, SMALL_PAGE_SIZE);
    const vaddr_t kernel_end = ALIGN_DOWN((uintptr_t)&_kernel_end, SMALL_PAGE_SIZE);

    ret->m_root = table_root;
    ret->m_phys_root = kmem_to_phys(nullptr, (vaddr_t)table_root);
    ret->m_kernel_low = kernel_start;
    ret->m_kernel_high = kernel_end;
    return 0;
}

/*!
 * @brief: Copy the contents of a pagemap into a new pagemap
 */
int kmem_copy_page_dir(page_dir_t* result, page_dir_t* src_dir)
{
    int error;
    pml_entry_t* dir;

    error = kmem_create_page_dir(result, NULL, NULL);

    if (error)
        return error;

    dir = src_dir->m_root;

    const uintptr_t dir_entry_limit = SMALL_PAGE_SIZE / sizeof(pml_entry_t);

    /* Don't go up into the kernel part */
    for (uintptr_t i = 0; i < (dir_entry_limit >> 1); i++) {

        /* Check the root pagedir entry (pml4 entry) */
        if (!pml_entry_is_bit_set(&dir[i], PDE_PRESENT))
            continue;

        paddr_t pml4_entry_phys = kmem_get_page_base(dir[i].raw_bits);
        pml_entry_t* pml4_entry = (pml_entry_t*)kmem_ensure_high_mapping(pml4_entry_phys);

        for (uintptr_t j = 0; j < dir_entry_limit; j++) {

            /* Check the second layer pagedir entry (pml3 entry) */
            if (!pml_entry_is_bit_set(&pml4_entry[j], PDE_PRESENT))
                continue;

            paddr_t pdp_entry_phys = kmem_get_page_base(pml4_entry[j].raw_bits);
            pml_entry_t* pdp_entry = (pml_entry_t*)kmem_ensure_high_mapping(pdp_entry_phys);

            for (uintptr_t k = 0; k < dir_entry_limit; k++) {

                /* Check the third layer pagedir entry (pml2 entry) */
                if (!pml_entry_is_bit_set(&pdp_entry[k], PDE_PRESENT))
                    continue;

                paddr_t pd_entry_phys = kmem_get_page_base(pdp_entry[k].raw_bits);
                pml_entry_t* pd_entry = (pml_entry_t*)kmem_ensure_high_mapping(pd_entry_phys);

                for (uintptr_t l = 0; l < dir_entry_limit; l++) {

                    /* Check the fourth layer pagedir entry (pml1 entry) */
                    if (!pml_entry_is_bit_set(&pd_entry[l], PTE_PRESENT))
                        continue;

                    /* Now we've reached the pagetable entry. If it's present we can mark it free */
                    paddr_t p_pt_entry = kmem_get_page_base(pd_entry[l].raw_bits);

                    // KLOG_DBG("kmem copying mapping p:0x%llx -> v:0x%llx\n", p_pt_entry, ((l << PAGE_SHIFT) | (k << (PAGE_SHIFT + 9)) | (j << (PAGE_SHIFT + 18)) | (i << (PAGE_SHIFT + 27))));

                    /* Map into the result root */
                    kmem_map_page(
                        result->m_root,
                        ((l << PAGE_SHIFT) | (k << (PAGE_SHIFT + 9)) | (j << (PAGE_SHIFT + 18)) | (i << (PAGE_SHIFT + 27))),
                        p_pt_entry,
                        KMEM_CUSTOMFLAG_GET_MAKE,
                        (pml_entry_is_bit_set(&pd_entry[l], PTE_USER) ? 0 : KMEM_FLAG_KERNEL) | (pml_entry_is_bit_set(&pd_entry[l], PTE_WRITABLE) ? KMEM_FLAG_WRITABLE : 0));
                } // pt loop

            } // pd loop

        } // pdp loop

    } // pml4 loop

    return 0;
}

void kmem_copy_bytes_into_map(vaddr_t vbase, void* buffer, size_t size, pml_entry_t* map)
{

    // paddr_t entry = kmem_to_phys(map, vbase);

    /* Mapping bytes into a virtial address of a different pmap */

    /*
     * TODO:  step 1 -> find physical address
     *        step 2 -> map into current map
     *        step 3 -> copy bytes
     *        step 4 -> unmap in current map
     *        step 5 -> move over to the next page
     */

    kernel_panic("TODO: implement kmem_copy_bytes_into_map");
}

/*!
 * @brief Copy the high half of pml4 into the page directory root @new_table
 *
 * Nothing to add here...
 */
int kmem_copy_kernel_mapping(pml_entry_t* new_table)
{
    pml_entry_t* kernel_root;

    const uintptr_t kernel_pml4_idx = 256;
    const uintptr_t end_idx = 511;

    kernel_root = (void*)kmem_from_phys(kmem_get_kernel_root_pd_phys(), KMEM_DATA.m_high_page_base);

    for (uintptr_t i = kernel_pml4_idx; i <= end_idx; i++)
        new_table[i].raw_bits = kernel_root[i].raw_bits;

    return (0);
}

/*!
 * @brief Clear the shared kernel mappings from a page directory
 *
 * We share everything from pml4 index 256 and above with user processes.
 */
static int __clear_shared_kernel_mapping(pml_entry_t* dir)
{
    const uintptr_t kernel_pml4_idx = 256;
    const uintptr_t end_idx = 512;

    for (uintptr_t i = kernel_pml4_idx; i < end_idx; i++)
        dir[i].raw_bits = NULL;

    return (0);
}

/*
 * TODO: do we want to murder an entire addressspace, or do we just want to get rid
 * of the mappings?
 * FIXME: with the current implementation we are definately bleeding memory, so another
 * TODO: plz fix the resource allocator asap so we can systematically find what mapping we can
 *       murder and which we need to leave alive
 *
 * NOTE: Keep this function in mind, it might be really buggy, indicated by the comments above written
 * by me from 1 year ago or something lmao
 */
int kmem_destroy_page_dir(pml_entry_t* dir)
{
    // if (__is_current_pagemap(dir))
    //   return -1;

    /*
     * We don't care about the result here
     * since it only fails if there is no kernel
     * entry, which is fine
     */
    __clear_shared_kernel_mapping(dir);

    /*
     * How do we want to clear our mappings?
     * 1) We recursively go through each level in the pagetable.
     *    when we find an entry that is presens and its not at the bottom
     *    level of the pagetable, we go a level deeper and recurse
     * 2) At each mapping, we check if its shared of some sort (aka, we should not clear it)
     *    if it is not shared, we can clear the mapping
     * 3) When we have cleared an entire pde, pte, ect. we can free up that page. Since a pagetable is just a tree of
     *    pages that contain mappings, we should make sure we find all the pages that contain clearable mappings,
     *    so that we can return them to the physical allocator, for them to be reused. Otherwise we
     *    keep these actually unused pages marked as 'used' by the allocator and we thus bleed memory
     *    which is not so nice =)
     */

    const uintptr_t dir_entry_limit = SMALL_PAGE_SIZE / sizeof(pml_entry_t);

    for (uintptr_t i = 0; i < (dir_entry_limit >> 1); i++) {

        /* Check the root pagedir entry (pml4 entry) */
        if (!pml_entry_is_bit_set(&dir[i], PDE_PRESENT))
            continue;

        paddr_t pml4_entry_phys = kmem_get_page_base(dir[i].raw_bits);
        pml_entry_t* pml4_entry = (pml_entry_t*)kmem_ensure_high_mapping(pml4_entry_phys);

        for (uintptr_t j = 0; j < dir_entry_limit; j++) {

            /* Check the second layer pagedir entry (pml3 entry) */
            if (!pml_entry_is_bit_set(&pml4_entry[j], PDE_PRESENT))
                continue;

            paddr_t pdp_entry_phys = kmem_get_page_base(pml4_entry[j].raw_bits);
            pml_entry_t* pdp_entry = (pml_entry_t*)kmem_ensure_high_mapping(pdp_entry_phys);

            for (uintptr_t k = 0; k < dir_entry_limit; k++) {

                /* Check the third layer pagedir entry (pml2 entry) */
                if (!pml_entry_is_bit_set(&pdp_entry[k], PDE_PRESENT))
                    continue;

                paddr_t pd_entry_phys = kmem_get_page_base(pdp_entry[k].raw_bits);
                pml_entry_t* pd_entry = (pml_entry_t*)kmem_ensure_high_mapping(pd_entry_phys);

                for (uintptr_t l = 0; l < dir_entry_limit; l++) {

                    /* Check the fourth layer pagedir entry (pml1 entry) */
                    if (!pml_entry_is_bit_set(&pd_entry[l], PTE_PRESENT))
                        continue;

                    /* Now we've reached the pagetable entry. If it's present we can mark it free */
                    paddr_t p_pt_entry = kmem_get_page_base(pd_entry[l].raw_bits);
                    uintptr_t idx = kmem_get_page_idx(p_pt_entry);
                    kmem_phys_dealloc_page(idx);
                } // pt loop

                /* Clear the pagetable */
                memset(pd_entry, 0, SMALL_PAGE_SIZE);

                uintptr_t idx = kmem_get_page_idx(pd_entry_phys);
                kmem_phys_dealloc_page(idx);

            } // pd loop

            uintptr_t idx = kmem_get_page_idx(pdp_entry_phys);
            kmem_phys_dealloc_page(idx);

        } // pdp loop

        uintptr_t idx = kmem_get_page_idx(pml4_entry_phys);
        kmem_phys_dealloc_page(idx);

    } // pml4 loop

    paddr_t dir_phys = kmem_to_phys_aligned(nullptr, (uintptr_t)dir);
    uintptr_t idx = kmem_get_page_idx(dir_phys);

    kmem_phys_dealloc_page(idx);

    return (0);
}

error_t kmem_set_addrspace_ex(page_dir_t* dir, struct proc* proc)
{
    /* Load the kernel map if there was no map specified */
    if (!dir)
        dir = &KMEM_DATA.kernel_pdir;

    /* If there whas a thread specified, update it's current active page directory */
    if (proc)
        memcpy(&proc->m_root_pd, dir, sizeof(*dir));

    ASSERT(get_current_processor() != nullptr);
    get_current_processor()->m_page_dir = dir;

    asm volatile("" : : : "memory");
    asm volatile("movq %0, %%cr3" ::"r"(dir->m_phys_root));
    asm volatile("" : : : "memory");

    return 0;
}

/*
 * NOTE: caller needs to ensure that they pass a physical address
 * as page map. CR3 only takes physical addresses
 */
error_t kmem_set_addrspace(page_dir_t* dir)
{
    /* Don't specify a thread */
    return kmem_set_addrspace_ex(dir, nullptr);
}

int kmem_get_kernel_address(vaddr_t* p_kaddr, vaddr_t virtual_address, pml_entry_t* map)
{
    return kmem_get_kernel_address_ex(p_kaddr, virtual_address, KMEM_DATA.m_high_page_base, map);
}

int kmem_get_kernel_address_ex(vaddr_t* p_kaddr, vaddr_t virtual_address, vaddr_t map_base, pml_entry_t* map)
{
    pml_entry_t* page;
    paddr_t p_address;
    vaddr_t v_kernel_address;
    vaddr_t v_align_delta;

    /* Can't grab from NULL */
    if (!map)
        return -1;

    /* Make sure we don't make a new page here */
    if (kmem_get_page(&page, map, virtual_address, 0, 0))
        return -1;

    p_address = kmem_get_page_base(page->raw_bits);

    /* Calculate the delta of the virtual address to its closest page base downwards */
    v_align_delta = virtual_address - ALIGN_DOWN(virtual_address, SMALL_PAGE_SIZE);

    /*
     * FIXME: 'high' mappings have a hard limit in them, so we will have to
     * create some kind of dynamic mapping for certain types of memory.
     * For example:
     *  - we map driver memory at a certain base for driver memory * the driver index
     *  - we map kernel resources at a certain base just for kernel resources
     *  - ect.
     */
    v_kernel_address = kmem_from_phys(p_address, map_base);

    /* Make sure the return the correctly aligned address */
    *p_kaddr = (v_kernel_address + v_align_delta);
    return 0;
}

/*!
 * @brief: Map a non-kernel address to the kernel map
 *
 * This will let the kernel access the same physical memory as a certain userprocess
 */
int kmem_map_to_kernel(vaddr_t* p_kaddr, vaddr_t uaddr, size_t size, vaddr_t map_base, pml_entry_t* map, u32 custom_flags, u32 kmem_flags)
{
    pml_entry_t* page;
    paddr_t p_address;
    vaddr_t v_kernel_address;
    vaddr_t v_align_delta;

    /* Can't grab from NULL */
    if (!map)
        return -1;

    /* Make sure we don't make a new page here */
    if (kmem_get_page(&page, map, uaddr, 0, 0))
        return -1;

    p_address = kmem_get_page_base(page->raw_bits);

    /* Calculate the delta of the virtual address to its closest page base downwards */
    v_align_delta = uaddr - ALIGN_DOWN(uaddr, SMALL_PAGE_SIZE);

    /*
     * FIXME: 'high' mappings have a hard limit in them, so we will have to
     * create some kind of dynamic mapping for certain types of memory.
     * For example:
     *  - we map driver memory at a certain base for driver memory * the driver index
     *  - we map kernel resources at a certain base just for kernel resources
     *  - ect.
     */
    v_kernel_address = kmem_from_phys(p_address, map_base);

    /* Try to map the address */
    kmem_map_range(nullptr, v_kernel_address, p_address, GET_PAGECOUNT(uaddr, size), custom_flags | KMEM_CUSTOMFLAG_GET_MAKE, kmem_flags);

    /* Make sure the return the correctly aligned address */
    *p_kaddr = (v_kernel_address + v_align_delta);
    return 0;
}

/*
 * TODO: Implement a lock around the physical allocator
 * FIXME: this could be heapless
 */
error_t init_kmem(uintptr_t* mb_addr)
{
    memset(&KMEM_DATA, 0, sizeof(KMEM_DATA));

    /* Create our mutex */
    _kmem_map_lock = create_mutex(NULL);

    /* Initialize the kernel physical memory unit */
    if (init_kmem_phys(mb_addr))
        return -ENULL;

    // Perform multiboot finalization
    finalize_multiboot();

    _init_kmem_page_layout();

    KMEM_DATA.m_kmem_flags |= KMEM_STATUS_FLAG_DONE_INIT;

    return 0;
}

