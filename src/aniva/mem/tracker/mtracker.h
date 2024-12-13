#ifndef __ANIVA_MEM_MTRACKER_H__
#define __ANIVA_MEM_MTRACKER_H__

#include "mem/kmem.h"
#include "mem/zalloc/zalloc.h"
#include "sync/mutex.h"
#include <lightos/types.h>

/* Single 'tracker' object is simply a page_allocation linked list, sorted by page_idx */
typedef u64 page_tracker_id_t;

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

typedef struct page_range {
    /*
     * We need this to be 52-bit in order to be able to reference
     * the entire 64-bit (maybe virtual) page range
     */
    vaddr_t page_idx : 52;
    u16 flags : 12;
    /* Epic */
    u32 nr_pages;
    /* How often is this shit referenced */
    u32 refc;
    /*
     * ID of the tracker we're referencing, with 0 being the
     * kernels physical range allocator and the default most times
     */
    page_tracker_id_t referenced_tid;
} page_range_t;

static inline void* page_range_to_ptr(page_range_t* range)
{
    /*
     * Calculates the (either virtual or physical) address from the page index,
     * which may be a virtual page index, or a physical page index.
     */
    return (void*)((u64)range->page_idx << PAGE_SHIFT);
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

typedef struct page_tracker {
    /* Base allocation list pointer */
    page_allocation_t* base;
    /*
     * This tracker ID. Used to reference trackers by using
     * a single 64-bit integer.
     * Having a reference 0
     */
    page_tracker_id_t id;
    /* Main lock that protects this boi =) */
    mutex_t* lock;
    /* Quick cache for getting allocation structs from */
    zone_allocator_t* allocation_cache;
} page_tracker_t;

/*
 * 'High' level functions we want to be able to use throughout the kernel
 *
 * We're going to use copies of the page_allocation struct everywhere to represent an allocation. This way
 * we stimulate clean allocation handling
 */
error_t page_tracker_alloc_any(page_tracker_t* tracker, size_t size, u32 flags, page_range_t* prange);
error_t page_tracker_dealloc(page_tracker_t* tracker, page_range_t* range);
error_t page_tracker_map(page_tracker_t* tracker, page_tracker_t* src, page_range_t* range);
error_t page_tracker_unmap(page_tracker_t* tracker, page_tracker_t* src, page_range_t* range);

/*
   Epic Thoughts dump

    Proc A
        -> range 0 -> 4 (p: 0->4)   (refc = 1)
        -> range 5 -> 6 (p: 5->6)  (refc = 1)
        -> range 8 -> 10 (p: 8->10) (refc = 2)
            -> if this range gets unmapped, before proc B drops its reference, we won't release the physical pages
               just yet, since they are still referenced by proc B
            ->

    Proc B

        -> range 0 -> 3 (p:11->14)        (refc = 1)
        -> range 4 -> 6 (p: 15 -> 17)     (refc = 1)
        (Shared memory region)
        -> range (From proc A) 9 -> 11 (p: 8->10) (refc = 1, tracker_id = A)
            -> When this range gets allocated, a call gets made to page_tracker_map(proc A, range, ...)
               this then recursively adds references to the mappings, constantly following the referenced_tid
               field in each range, until we hit the physical allocator (referenced_tid == 0).
               At this point we've left our trace and we can start using the range.
            -> When a third proc (let's say Proc C), wants to map this range, but now from OUR tracker,
               it looks into our mapping, adds one reference, follows referenced_tid, gets to Proc A, adds
               one reference, follows referenced_tid again and reaches the physical allocator, where Proc A
               initially mapped the range.
            -> When Proc B wants to unmap it's shared range from Proc A, but Proc C has it's shared range still
               mapped, we can simply remove refc (from Proc B) references from Proc A, since any reference to a
               reference, is implied as a 'direct reference'. This way, Proc C gets consistent mapping behaviour.
               it now references a tracker that doesn't have its referenced mapping anymore, so we'll have to
               account for this.

    A)
    Proc A (1) -> physical (1)

    B)
    Proc B (1) -> Proc A (2) -> physical (2)

    C)
    Proc C (1) -> Proc B (2) -> Proc A (3) -> physical (3)

    D)
    Proc C (1) -> ????? -> Proc A (1) -> physical (1)

    We can avoid this weird behaviour by doing this:

    A)
    Proc A (1) -> physical (1)

    B)
    Proc A (1) ->
                   physical (2)
    Proc B (1) ->
*/

#endif // !__ANIVA_MEM_MTRACKER_H__
