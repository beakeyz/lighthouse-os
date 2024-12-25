#include "tracker.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "mem/zalloc/zalloc.h"
#include "sync/spinlock.h"
#include <libk/math/math.h>

/*!
 * @brief: Initialize a signel page tracker object
 */
error_t init_page_tracker(page_tracker_t* tracker, void* range_cache_buf, size_t bsize)
{
    error_t error;

    if (!tracker)
        return -ENULL;

    memset(tracker, 0, sizeof(*tracker));

    /* Initialize the spinloc */
    init_spinlock(&tracker->lock, NULL);

    /* Try to initialize the zone allocator */
    error = init_zone_allocator_ex(&tracker->allocation_cache, range_cache_buf, bsize, sizeof(page_allocation_t), NULL);

    /* FUck */
    if (error)
        return error;

    return 0;
}

/*!
 * @brief: Destroy a page tracker and all it's tracked memory
 *
 * TODO: Implement
 */
error_t destroy_page_tracker(page_tracker_t* tracker)
{
    kernel_panic("TODO: destroy_page_tracker");
    return 0;
}

error_t page_tracker_dump(page_tracker_t* tracker)
{
    u32 idx = 1;
    for (page_allocation_t* a = tracker->base; a; a = a->nx_link) {
        KLOG_DBG("(%d): From Page: %lld (Size: %lld). Flag=%d, Refs=%d\n", idx, a->range.page_idx, a->range.nr_pages, a->range.flags, a->range.refc);

        idx++;
    }

    return 0;
}

static inline bool __range_contains_idx(page_range_t* range, u64 page)
{
    return (range->page_idx >= page && (range->page_idx + range->nr_pages) < page);
}

/*!
 * @brief: Quickly get the middle range
 *
 * Implements a simple slow-fast pointer algorithm
 */
static inline page_allocation_t* __get_middle_range(page_allocation_t* start, page_allocation_t* end)
{
    page_allocation_t *fast, *slow;

    /* Initialize both pointers at the start */
    fast = slow = start;

    /*
     * By the time fast reaches @end, slow has reached the middle between @start
     * and @end If fast is null or fast has no next, we're at the end of the link
     * =/
     */
    while (fast && fast->nx_link && fast != end && fast->nx_link != end) {
        slow = slow->nx_link;
        fast = fast->nx_link->nx_link;
    }

    return slow;
}

static page_allocation_t* __page_allocation_append(page_tracker_t* tracker, page_allocation_t* p_alloc_slot, page_range_t* range)
{
    page_allocation_t* new_allocation;

    if (!tracker || !p_alloc_slot || !range)
        return nullptr;

    if (!range->nr_pages)
        return nullptr;

    /* Allocate a new allocation */
    new_allocation = zalloc_fixed(&tracker->allocation_cache);

    if (!new_allocation)
        return nullptr;

    KLOG_DBG("Adding also idx: %lld, nr: %d\n", range->page_idx, range->nr_pages);

    /* Initialize the range */
    new_allocation->range = *range;

    /* Complete the link */
    new_allocation->nx_link = p_alloc_slot->nx_link;
    p_alloc_slot->nx_link = new_allocation;

    return new_allocation;
}

static error_t __page_allocation_remove(page_tracker_t* tracker, page_allocation_t* alloc)
{
    page_allocation_t** a;

    /* TODO: Replace this linear scan with something not O(n) xD */
    for (a = &tracker->base; *a; a = &(*a)->nx_link) {
        if (alloc == *a)
            break;
    }

    KLOG_DBG("Removing idx: %lld, len: %d\n", alloc->range.page_idx, alloc->range.nr_pages);

    /* Solve the link here */
    *a = alloc->nx_link;
    /* Clear the pointer for safety */
    alloc->nx_link = nullptr;

    /* Free the allocation */
    zfree_fixed(&tracker->allocation_cache, alloc);

    return 0;
}

static error_t __page_allocation_append_in_slot(page_tracker_t* tracker, page_allocation_t** p_alloc_slot, page_range_t* range)
{
    page_allocation_t* new_allocation;

    if (!tracker || !p_alloc_slot || !range)
        return -EINVAL;

    /* Allocate a new allocation */
    new_allocation = zalloc_fixed(&tracker->allocation_cache);

    if (!new_allocation)
        return -ENOMEM;

    /* Initialize the range */
    new_allocation->range = *range;

    /* Complete the link */
    new_allocation->nx_link = *p_alloc_slot;
    *p_alloc_slot = new_allocation;

    return 0;
}

/*!
 * @brief: Get the closest range inside the tracker to @page_idx
 *
 * Should be an algorithm of O(log(n)), since it cuts the ranges in half each
 * itteration
 *
 */
static error_t __tracker_get_closest_range(page_tracker_t* tracker, u64 page_idx, page_allocation_t** range, page_allocation_t** prev_range)
{
    page_allocation_t *start, *end, *target;

    /* Check validity */
    if (!tracker || !tracker->base)
        return -EINVAL;

    /* Initialize the bounds */
    start = tracker->base;
    end = nullptr;

    /* Itterate the ranges */
    do {
        target = __get_middle_range(start, end);

        /* Also break if the target is stuck on one bound */
        if (target == start || target == end)
            break;

        /* Check how we shift the bounds */
        if (page_idx > ((u64)target->range.page_idx + target->range.nr_pages))
            start = target;
        else
            end = target;
    } while (!__range_contains_idx(&target->range, page_idx) && start != end);

    /* If this range isn't in our target, find the range with the smallest delta
     */
    if (__range_contains_idx(&target->range, page_idx))
        goto export_and_return;

    u64 best_delta = page_range_get_distance_to_page(&target->range, page_idx);
    u64 delta;
    page_allocation_t* prev = nullptr;

    /* Find the range with is closest to @page_idx */
    for (page_allocation_t* a = start; a != (!end ? nullptr : end->nx_link); a = a->nx_link) {
        delta = page_range_get_distance_to_page(&a->range, page_idx);

        /* Check if this range has a smaller distance to our target */
        if (delta >= best_delta)
            goto cycle;

        best_delta = delta;
        target = a;

        /* Export this already */
        if (prev_range && prev)
            *prev_range = prev;

    cycle:
        /* Record the previous basterd */
        prev = a;
    }

export_and_return:
    /* Export that range */
    *range = target;

    return 0;
}

/*!
 * @brief: Check if we can prepend a part of @range to alloc
 *
 * This might happen when @alloc is at tracker->base and there is a part of @range that sticks out in front of
 * @alloc. In this case, we simply want to prepend a range in front of alloc, such that tracker->base points to
 * that range.
 */
static inline error_t __check_base_prepend(page_tracker_t* tracker, page_allocation_t* alloc, page_range_t* range)
{
    page_range_t new_range = { 0 };

    if (tracker->base != alloc)
        return -EINVAL;

    if (range->page_idx >= alloc->range.page_idx)
        return -EINVAL;

    /* Initialize a new range */
    init_page_range(&new_range, range->page_idx, (alloc->range.page_idx - range->page_idx), range->flags, range->refc);

    /* Append the bastard */
    __page_allocation_append_in_slot(tracker, &tracker->base, &new_range);

    /* Shrink the old range */
    page_range_shrink(range, new_range.nr_pages);

    return 0;
}

/*!
 * @brief: Tries to cut out a single page range into multiple parts
 *
 * @plast: (new) last allocation we got from slicing @alloc into multiple parts
 */
static error_t __cut_out_range_from_allocation(page_tracker_t* tracker, page_allocation_t* alloc, page_range_t* range, page_allocation_t** plast)
{
    page_allocation_t* last = nullptr;
    /* Calculate which start idx is the highest */
    const size_t highest_start_page_idx = MAX(alloc->range.page_idx, range->page_idx);
    /* Calculate which end idx is the lowest */
    const size_t lowest_end_page_idx = MIN(alloc->range.page_idx + alloc->range.nr_pages,
        range->page_idx + range->nr_pages);
    /* Now we can calculate how much of @range is inside @alloc */
    const size_t page_delta = lowest_end_page_idx < highest_start_page_idx
        ? 0
        : (lowest_end_page_idx - highest_start_page_idx);

    /* If there is a page delta of 0, there is no part of @range inside @alloc */
    if (!page_delta)
        return -ENOENT;

    /* In this case we simply can't cut out a range =( */
    if (highest_start_page_idx != range->page_idx && __check_base_prepend(tracker, alloc, range))
        return -EINVAL;

    KLOG_DBG("Delta: %lld\n", page_delta);
    KLOG_DBG("Low end: %lld, High end: %lld\n", lowest_end_page_idx, highest_start_page_idx);

    /*
     * If the delta is equal to the number of pages in @alloc, it means @range
     * completely coverst @alloc, so we can just increment alloc->range.refc
     * and dip
     */
    if (page_delta == alloc->range.nr_pages) {
        alloc->range.refc += range->refc;

        goto update_range_and_exit;
    }

    /*
     * In this case, range is somewhere inside @alloc. We might need to split
     * alloc in two, and insert a new range the size of @range
     */
    if (page_delta <= range->nr_pages) {
        /* Create a new range that describes @range */
        page_range_t new_range = {
            .page_idx = range->page_idx,
            .nr_pages = page_delta,
            /* Add a refcount to range, since it now describes @alloc AND @range
               itself */
            .refc = range->refc + alloc->range.refc,
            .flags = range->flags,
        };

        /* New range for the end split of @alloc */
        page_range_t alloc_end_split = {
            .page_idx = range->page_idx + range->nr_pages + 1,
            .nr_pages = (alloc->range.page_idx + alloc->range.nr_pages) - lowest_end_page_idx,
            .refc = alloc->range.refc,
            .flags = alloc->range.flags,
        };

        /* First append the end split */
        if (alloc_end_split.nr_pages)
            last = __page_allocation_append(tracker, alloc, &alloc_end_split);

        /* Then append @range, so it ends up before alloc_end_split */
        page_allocation_t* last_2 = __page_allocation_append(tracker, alloc, &new_range);

        /* Check which last we need to set (This is sooo ugly ;-;) */
        if (plast)
            *plast = (last == nullptr) ? last_2 : last;

        /* Cut off the alloc range */
        alloc->range.nr_pages -= (alloc_end_split.nr_pages + new_range.nr_pages);
        /* If @range sits at the front of @alloc, we need to remove alloc */
        if (highest_start_page_idx == alloc->range.page_idx || alloc->range.nr_pages == 0)
            __page_allocation_remove(tracker, alloc);
    }

update_range_and_exit:
    /* Update the ranges bounds */
    page_range_shrink(range, page_delta);

    return 0;
}

static inline bool __page_range_can_extend(page_range_t* first, page_range_t* last)
{
    KLOG_DBG("Can merge? (%lld -> %lld, %lld + %lld), refs=(%lld, %lld), flags=(%lld, %lld)\n",
        first->page_idx, last->page_idx, first->nr_pages, last->nr_pages,
        first->refc, last->refc, first->flags, last->flags);
    /* Bordering ranges with the same number of references and the same flags may
     * merge */
    return ((first->page_idx + first->nr_pages) == last->page_idx && first->refc == last->refc && first->flags == last->flags);
}

static error_t __page_tracker_add_allocation(page_tracker_t* tracker, u64 page_idx, u64 page_count, u32 flags)
{
    error_t error;
    size_t target_size_pgs;
    page_range_t range;
    page_range_t this_range;
    page_allocation_t* next;
    page_allocation_t* last;
    page_allocation_t* start = NULL;
    page_allocation_t* prev_start = NULL;

    /* Initialize a single range */
    range.page_idx = page_idx;
    range.nr_pages = page_count;
    range.flags = flags;
    range.refc = 1;

    KLOG_DBG("A\n");

    /* If there are no ranges yet, we can just append this one at the base */
    if (!tracker->base)
        return __page_allocation_append_in_slot(tracker, &tracker->base, &range);

    KLOG_DBG("Trying to get closest\n");

    /* Find the range closest to @page_idx */
    error = __tracker_get_closest_range(tracker, page_idx, &start, &prev_start);

    if (error)
        return error;

    KLOG_DBG("got closest\n");

    /* Check which range we use as the start */
    if (page_idx < start->range.page_idx && prev_start)
        start = prev_start;

    KLOG_DBG("Trying to cut out ranges...\n");

    /*
     * Loop over all allocations until we've depleted @range
     * What are the options:
     * 1) @range is completely in between two ranges. Simply append a new range
     * 2) @range is right after a certain range, extend that range
     * 3) @range is (partially) in a certain range. Cut out the part thats inside
     * and increment refc
     */
    for (page_allocation_t* a = start; a != nullptr; a = next) {
        /* Specify the next allocation early, so we don't mess up when adding new
         * allocations */
        next = a->nx_link;
        /* Store a copy of the current range */
        this_range = a->range;
        /* Pre-set last */
        last = a;

        KLOG_DBG("Cutting in range idx: %lld, pages: %d...\n", a->range.page_idx, a->range.nr_pages);

        /* First, check if @range is (partially) inside @a */
        (void)__cut_out_range_from_allocation(tracker, a, &range, &last);

        /* Spent the entire range, let's dip */
        if (!range.nr_pages)
            break;

        target_size_pgs = MIN(
            range.nr_pages, !next ? (u32)-1 : (next->range.page_idx - (this_range.page_idx + this_range.nr_pages)));

        KLOG_DBG("Pages left: %lld\n", target_size_pgs);

        /* Second, check how much space if free after @a */
        page_range_t new_range = {
            .page_idx = range.page_idx,
            .nr_pages = target_size_pgs,
            .refc = range.refc,
            .flags = range.flags,
        };

        /* Check if we can extend  */
        if (__page_range_can_extend(&last->range, &new_range))
            /* Extend the last range */
            last->range.nr_pages += new_range.nr_pages;
        else
            __page_allocation_append(tracker, last, &new_range);

        /* Last, mutate @range by removing some of its size according to how we
         * added it to the tracker already and cycle */
        page_range_shrink(&range, target_size_pgs);
    }

    KLOG_DBG("Trying to expand ranges...\n");

    /* Walk the loop again, to remove unneeded ranges */
    for (page_allocation_t* a = start; a != nullptr && a != last; a = next) {
        /* Pre-set the next link */
        next = a->nx_link;

        /* Make sure we're alive */
        if (!next)
            break;

        if (!__page_range_can_extend(&a->range, &next->range))
            continue;

        /* 'Merge' lol */
        a->range.nr_pages += next->range.nr_pages;

        /* Set the next next guy */
        next = next->nx_link;

        /* Remove this bastard */
        __page_allocation_remove(tracker, a->nx_link);
    }

    return error;
}

/*!
 * @brief: Allocate any range
 */
error_t page_tracker_alloc_any(page_tracker_t* tracker, enum PAGE_TRACKER_ALLOC_MODE mode, size_t nr_pages, u32 flags, page_range_t* prange)
{
    size_t page_delta;
    page_range_t this_range;
    page_allocation_t* cur_range;

    /* Check the parameters */
    if (!tracker || !nr_pages || !prange)
        return -EINVAL;

    /* Init our page range */
    init_page_range(&this_range, 0, nr_pages, flags, 1);

    /* If there is no base left, we can just yeet in this range anywhere */
    if (!tracker->base) {
        __page_allocation_append_in_slot(tracker, &tracker->base, &this_range);

        /* Export the fucker */
        *prange = this_range;
        return 0;
    }

    /* Loop over all the ranges to check if there is any space inbetween them */
    for (cur_range = tracker->base; cur_range; cur_range = cur_range->nx_link) {

        /* If there is no range after this boi, we can just stick if after it */
        if (!cur_range->nx_link)
            break;

        /* Calculate the page delta */
        page_delta = cur_range->nx_link->range.page_idx - ((u64)cur_range->range.page_idx + cur_range->range.nr_pages);

        /* If we found a delta that has space and we're trying to find the first
         * fit, break */
        if (page_delta >= nr_pages && mode == PAGE_TRACKER_FIRST_FIT)
            break;
    }

    /* Couldn't find a range to stick this after */
    if (!cur_range)
        return -ENOSPC;

    /* Set the page index of this range */
    this_range.page_idx = (cur_range->range.page_idx + cur_range->range.nr_pages);

    /* Check if we can just extend the current range */
    if (__page_range_can_extend(&cur_range->range, &this_range))
        /* Just extend this fucker if the ranges have the same attributes */
        cur_range->range.nr_pages += nr_pages;
    else
        /* Append a range if they don't match */
        __page_allocation_append(tracker, cur_range, &this_range);

    return 0;
}

error_t page_tracker_alloc(page_tracker_t* tracker, u64 start_page, size_t nr_pages, u32 flags)
{
    error_t error;

    spinlock_lock(&tracker->lock);

    error = __page_tracker_add_allocation(tracker, start_page, nr_pages, flags);

    spinlock_unlock(&tracker->lock);

    return error;
}

error_t page_tracker_dealloc(page_tracker_t* tracker, page_range_t* range)
{
    kernel_panic("TODO: dealloc");
    return 0;
}

/*!
 * @brief: Check if the tracker contains a certain address
 *
 * If it does, there is a range that contains the address. We need to find
 * this range. If we can find it, set @phas to true. Otherwise set it to false.
 */
error_t page_tracker_has_addr(page_tracker_t* tracker, u64 addr, bool* phas)
{
    if (!tracker || !phas)
        return -EINVAL;

    if (page_tracker_get_range(tracker, kmem_get_page_idx(addr), NULL))
        *phas = true;
    else
        *phas = false;

    return 0;
}

error_t page_tracker_get_range(page_tracker_t* tracker, u64 page_idx, page_range_t* range)
{
    error_t error;
    page_allocation_t* prange;

    error = __tracker_get_closest_range(tracker, page_idx, &prange, nullptr);

    /* ??? */
    if (error)
        return error;

    /* Check if bro contains this index */
    if (!__range_contains_idx(&prange->range, page_idx))
        return -ENOENT;

    /* Copy out the range */
    if (range)
        *range = prange->range;

    return 0;
}

error_t page_tracker_copy(page_tracker_t* src, page_tracker_t* dest)
{
    return 0;
}
