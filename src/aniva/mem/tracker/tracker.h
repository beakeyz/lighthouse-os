#ifndef __ANIVA_MEM_tracker_H__
#define __ANIVA_MEM_tracker_H__

#include "mem/kmem.h"
#include "mem/zalloc/zalloc.h"
#include <lightos/api/memory.h>

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
    mutex_t lock;
    /* Store the cache buffer here... */
    void* cache_buffer;
    /* and the size of that buffer */
    size_t sz_cache_buffer;
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
    PAGE_TRACKER_FIRST_FIT,
    PAGE_TRACKER_LAST_FIT,
    PAGE_TRACKER_BEST_FIT,
    PAGE_TRACKER_WORST_FIT,
};

/*
 * 'High' level functions we want to be able to use throughout the kernel
 *
 * We're going to use copies of the page_allocation struct everywhere to
 * represent an allocation. This way we stimulate clean allocation handling
 */
error_t page_tracker_alloc_any(page_tracker_t* tracker, enum PAGE_TRACKER_ALLOC_MODE mode, size_t nr_pages, u32 flags, page_range_t* prange);
error_t page_tracker_find_fitting_range(page_tracker_t* tracker, enum PAGE_TRACKER_ALLOC_MODE mode, size_t nr_pages, page_range_t* prange);
error_t page_tracker_alloc(page_tracker_t* tracker, u64 start_page, size_t nr_pages, u32 flags);
error_t page_tracker_dealloc(page_tracker_t* tracker, page_range_t* range);
error_t page_tracker_has_addr(page_tracker_t* tracker, u64 addr, bool* phas);
error_t page_tracker_get_range(page_tracker_t* tracker, u64 page_idx, page_range_t* range);
error_t page_tracker_copy(page_tracker_t* src, page_tracker_t* dest);
error_t page_tracker_get_used_page_count(page_tracker_t* tracker, size_t* pcount);

#endif // !__ANIVA_MEM_tracker_H__
