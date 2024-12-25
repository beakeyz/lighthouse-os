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
#define PAGE_RANGE_PGE_IDX_MASK 0x000fffffffffffffULL
#define PAGE_RANGE_FLAGS_MASK 0x0fff

typedef struct page_range {
    /*
     * We need this to be 52-bit in order to be able to reference
     * the entire 64-bit (maybe virtual) page range
     */
    union {
        struct {
            vaddr_t page_idx : 52;
            u16 flags : 12;
        };
        u64 attr;
    };
    /* Epic */
    u32 nr_pages;
    /* How often is this shit referenced */
    u32 refc;
} page_range_t;

static inline error_t init_page_range(page_range_t* range, vaddr_t page_idx, u32 nr_pages, u16 flags, u32 refc)
{
    if ((flags & ~PAGE_RANGE_FLAGS_MASK) != 0)
        return -EINVAL;

    /* Check for index bounds */
    if ((page_idx & ~PAGE_RANGE_PGE_IDX_MASK) != 0)
        return -EINVAL;

    range->page_idx = page_idx;
    range->nr_pages = nr_pages;
    range->flags = flags;
    range->refc = refc;

    return 0;
}

static inline void* page_range_to_ptr(page_range_t* range)
{
    /*
     * Calculates the (either virtual or physical) address from the page index,
     * which may be a virtual page index, or a physical page index.
     */
    return (void*)((u64)range->attr & ~PAGE_RANGE_FLAGS_MASK);
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
    /* Count the number of allocated pages */
    size_t nr_alloced_pages;
    /* Main lock that protects this boi =) */
    spinlock_t lock;
    /* Quick cache for getting allocation structs from */
    zone_allocator_t allocation_cache;
} page_tracker_t;

error_t init_page_tracker(page_tracker_t* tracker, void* range_cache_buf, size_t bsize);
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

#endif // !__ANIVA_MEM_tracker_H__
