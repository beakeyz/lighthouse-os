#ifndef __LIGHTOS_MEMORY_H__
#define __LIGHTOS_MEMORY_H__

/*
 * Flags for userspace memory attributes
 *
 * This header is also used inside the kernel
 */

#include <lightos/types.h>

/* The page shift the lightos platform uses to get to the page size. Might differ from the physical page shift */
#define LIGHTOS_PAGE_SHIFT 12

/* Minimum of 4 Kib in order to save pool integrity */
#define MIN_POOLSIZE (0x1000)
#define MEMPOOL_ALIGN (0x1000)
#define MEMPOOL_ENTSZ_ALIGN (8)

/*
 * Virtual memory flags
 */

/* This range of memory should be readable */
#define VMEM_FLAG_READ 0x00000001UL
/* This range of memory should be writable */
#define VMEM_FLAG_WRITE 0x00000002UL
/* This range of memory may contain executable code */
#define VMEM_FLAG_EXEC 0x00000004UL
/* Other processes may map the same (part of this) region of memory */
#define VMEM_FLAG_SHARED 0x00000008UL
/* Deletes the specified mapping */
#define VMEM_FLAG_DELETE 0x00000010UL
/* Remaps the specified range somewhere else */
#define VMEM_FLAG_REMAP 0x00000020UL
/* Asks for a page range to be bounded to a vmem object */
#define VMEM_FLAG_BIND 0x00000040UL

/*
 * Page range flags
 */

/* This range can be written to */
#define PAGE_RANGE_FLAG_WRITABLE 0x001
/* This range is claimed by the kernel (or kernel modules) */
#define PAGE_RANGE_FLAG_KERNEL 0x002
/* This range is exported and can by mapped by anyone */
#define PAGE_RANGE_FLAG_EXPORTED 0x004
/* This range may contain executable code */
#define PAGE_RANGE_FLAG_EXEC 0x008
/* This range isn't backed by anything */
#define PAGE_RANGE_FLAG_UNBACKED 0x010

/* Masks for page index and flags */
#define PAGE_RANGE_PGE_IDX_MASK 0xfffffffffffff000ULL
#define PAGE_RANGE_FLAGS_MASK 0x0fffULL

typedef struct page_range {
    /*
     * We need this to be 52-bit in order to be able to reference
     * the entire 64-bit (maybe virtual) page range
     */
    u64 page_idx;
    /* Epic */
    u32 nr_pages;
    /* How often is this shit referenced */
    u16 refc;
    u16 flags;
} page_range_t;

static inline bool page_range_is_backed(page_range_t* range)
{
    /* If the flag is clear, this range is backed */
    return (range->flags & PAGE_RANGE_FLAG_UNBACKED) != PAGE_RANGE_FLAG_UNBACKED;
}

static inline void* page_range_to_ptr(page_range_t* range)
{
    /*
     * Calculates the (either virtual or physical) address from the page index,
     * which may be a virtual page index, or a physical page index.
     */
    return (void*)((u64)(range->page_idx << LIGHTOS_PAGE_SHIFT) & PAGE_RANGE_PGE_IDX_MASK);
}

static inline size_t page_range_size(page_range_t* range)
{
    return ((size_t)range->nr_pages << LIGHTOS_PAGE_SHIFT);
}

static inline void init_page_range(page_range_t* range, vaddr_t page_idx, u32 nr_pages, u16 flags, u32 refc)
{
    range->page_idx = page_idx;
    range->nr_pages = nr_pages;
    range->flags = flags;
    range->refc = refc;
}

/*!
 * @brief: Shrinks @range forward by @pages pages
 *
 * @returns: True if the range is reduced to zero pages. False otherwise
 */
static inline bool page_range_shrink_from_start(page_range_t* range, size_t pages)
{
    if (!range->nr_pages)
        return true;

    if (pages > range->nr_pages)
        pages = range->nr_pages;

    /* Reduce the range */
    range->page_idx += pages;
    range->nr_pages -= pages;

    /* Check if there is space left */
    return (range->nr_pages == 0);
}

static inline size_t page_range_get_pages_inside(page_range_t* inside, page_range_t* outside)
{
    const size_t inside_end_page = inside->page_idx + inside->nr_pages;
    const size_t outsize_end_page = outside->page_idx + outside->nr_pages;

    /*
     * If @inside is, in fact, completely inside @outside, we can just return @inside's size.
     * If there is a bit peeping out, we need to subtract that part
     */
    return inside->nr_pages - (inside_end_page > outsize_end_page ? (inside_end_page - outsize_end_page) : 0);
}

static inline bool page_range_equal_bounds(page_range_t* a, page_range_t* b)
{
    return (a->page_idx == b->page_idx && a->nr_pages == b->nr_pages);
}

static inline size_t page_range_get_distance_to_page(page_range_t* range, u64 page)
{
    const size_t page_idx = range->page_idx;

    /* Page is before the range */
    if (page <= page_idx)
        return page_idx - page;

    /* Page is after the range */
    if (page > (page_idx + range->nr_pages - 1))
        return page - (page_idx + range->nr_pages - 1);

    return 0;
}

#endif // !__LIGHTOS_MEMORY_H__
