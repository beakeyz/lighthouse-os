#ifndef __ANIVA_MEM_tracker_H__
#define __ANIVA_MEM_tracker_H__

#include "mem/kmem.h"
#include "mem/zalloc/zalloc.h"
#include "sync/spinlock.h"
#include <lightos/types.h>

/* This range can be written to */
#define PAGE_RANGE_FLAG_WRITABLE 0x001
/* This range is claimed by the kernel (or kernel modules) */
#define PAGE_RANGE_FLAG_KERNEL 0x002
/* This range is exported and can by mapped by anyone */
#define PAGE_RANGE_FLAG_EXPORTED 0x004
/* This range may contain executable code */
#define PAGE_RANGE_FLAG_EXEC 0x008
/* This range describes a chunk of physical memory */
#define PAGE_RANGE_FLAG_PHYSICAL 0x010

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

static inline void* page_range_to_ptr(page_range_t* range)
{
    /*
     * Calculates the (either virtual or physical) address from the page index,
     * which may be a virtual page index, or a physical page index.
     */
    return (void*)((u64)(range->page_idx << PAGE_SHIFT) & PAGE_RANGE_PGE_IDX_MASK);
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

/*!
 * @brief: Structure to track page allocations
 *
 *
 */
typedef struct page_allocation {
    /* Range that describes our page range */
    page_range_t range;
    /* Next link */
    struct page_allocation* nx_link;
} page_allocation_t;

/*!
 * @brief: Structure that manages paged memory chunks
 *
 * This structure is essentially the core 'owner' of the memory. If it gets
 * destroyed, all the backing memory gets released (This might be simply
 * unmapping, or performing syncs on certain files).
 */
typedef struct page_tracker {
    /* Base allocation list pointer */
    page_allocation_t* base;
    /* The maximum addressable page in this tracker */
    u64 max_page;
    /* Main lock that protects this boi =) */
    spinlock_t lock;
    /* Quick cache for getting allocation structs from */
    zone_allocator_t allocation_cache;
} page_tracker_t;

error_t init_page_tracker(page_tracker_t* tracker, void* range_cache_buf, size_t bsize, u64 max_page);
error_t destroy_page_tracker(page_tracker_t* tracker);
error_t page_tracker_dump(page_tracker_t* tracker);

/*
 * Different allocation modes for
 * page_tracker_alloc_any
 */
enum PAGE_TRACKER_ALLOC_MODE {
    PAGE_TRACKER_BEST_FIT,
    PAGE_TRACKER_WORST_FIT,
    PAGE_TRACKER_FIRST_FIT,
    PAGE_TRACKER_LAST_FIT,
};

/*
 * 'High' level functions we want to be able to use throughout the kernel
 *
 * We're going to use copies of the page_allocation struct everywhere to
 * represent an allocation. This way we stimulate clean allocation handling
 */
error_t page_tracker_alloc_any(page_tracker_t* tracker, enum PAGE_TRACKER_ALLOC_MODE mode, size_t nr_pages, u32 flags, page_range_t* prange);
error_t page_tracker_alloc(page_tracker_t* tracker, u64 start_page, size_t nr_pages, u32 flags);
error_t page_tracker_dealloc(page_tracker_t* tracker, page_range_t* range);
error_t page_tracker_has_addr(page_tracker_t* tracker, u64 addr, bool* phas);
error_t page_tracker_get_range(page_tracker_t* tracker, u64 page_idx, page_range_t* range);
error_t page_tracker_copy(page_tracker_t* src, page_tracker_t* dest);
error_t page_tracker_get_used_page_count(page_tracker_t* tracker, size_t* pcount);

#endif // !__ANIVA_MEM_tracker_H__
