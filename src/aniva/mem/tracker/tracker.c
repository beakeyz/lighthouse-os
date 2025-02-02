#include "tracker.h"
#include "libk/flow/error.h"
#include "lightos/error.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "mem/zalloc/zalloc.h"
#include "sync/mutex.h"
#include <libk/math/math.h>

/*!
 * @brief: Initialize a signel page tracker object
 */
error_t init_page_tracker(page_tracker_t* tracker, void* range_cache_buf, size_t bsize, u64 max_page)
{
    error_t error;

    if (!tracker)
        return -ENULL;

    memset(tracker, 0, sizeof(*tracker));

    /* Initialize the spinloc */
    // init_mutex(&tracker->lock, NULL);
    init_mutex(&tracker->lock, NULL);

    /* Try to initialize the zone allocator */
    error = init_zone_allocator_ex(&tracker->allocation_cache, range_cache_buf, bsize, sizeof(page_allocation_t), NULL);

    /* FUck */
    if (error)
        return error;

    tracker->cache_buffer = range_cache_buf;
    tracker->sz_cache_buffer = bsize;
    tracker->max_page = max_page;

    return 0;
}

/*!
 * @brief: Destroy a page tracker and all it's tracked memory
 *
 * We don't need to destroy the mutex, since that's a self-sustaining structure for us
 */
error_t destroy_page_tracker(page_tracker_t* tracker)
{
    /* Destroy the zone allocator we used */
    destroy_zone_allocator(&tracker->allocation_cache, false);

    destroy_mutex(&tracker->lock);

    return 0;
}

error_t page_tracker_dump(page_tracker_t* tracker)
{
    KLOG_ERR("Dumping page tracker (0x%p):\n", tracker);

    u64 prev_range_terminating_page = 0;
    u32 idx = 1;

    for (page_allocation_t* a = tracker->base; a; a = a->nx_link) {
        u64 free_space_before = a->range.page_idx - prev_range_terminating_page;

        if (free_space_before)
            KLOG_ERR("  - Free range of %lld pages\n", free_space_before);

        KLOG_ERR("(%d): From Page: 0x%llx (Size: %lld). Flag=%d, Refs=%d\n", idx, a->range.page_idx, a->range.nr_pages, a->range.flags, a->range.refc);

        idx++;
        prev_range_terminating_page = (a->range.page_idx + a->range.nr_pages);
    }

    return 0;
}

static inline bool __range_contains_idx(page_range_t* range, u64 page)
{
    return (range->page_idx >= page && (range->page_idx + range->nr_pages - 1) <= page);
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

    /* Initialize the range */
    memcpy(&new_allocation->range, range, sizeof(*range));

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

    /* Solve the link here */
    *a = alloc->nx_link;
    /* Clear the pointer for safety */
    alloc->nx_link = nullptr;

    /* Free the allocation */
    zfree_fixed(&tracker->allocation_cache, alloc);

    return 0;
}

static error_t __page_allocation_deref(page_tracker_t* tracker, page_allocation_t* alloc, u32 refc)
{
    /* Prevent weird overflows */
    if (refc > alloc->range.refc)
        refc = alloc->range.refc;

    /* Remove these references */
    alloc->range.refc -= refc;

    /* If this boi still has references left, don't do anything */
    if (alloc->range.refc)
        return 0;

    /* No references left. Remove the fucker */
    return __page_allocation_remove(tracker, alloc);
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
        if (page_idx < target->range.page_idx)
            end = target;
        else
            start = target;
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
    init_page_range(&new_range, range->page_idx, MIN(alloc->range.page_idx - range->page_idx, range->nr_pages), range->flags, range->refc);

    /* Append the bastard */
    __page_allocation_append_in_slot(tracker, &tracker->base, &new_range);

    /* Shrink the old range */
    page_range_shrink_from_start(range, new_range.nr_pages);

    return 0;
}

/*!
 * @brief: Tries to cut out a single page range into multiple parts
 *
 * @plast: (new) last allocation we got from slicing @alloc into multiple parts
 */
static error_t __cut_out_range_from_allocation(page_tracker_t* tracker, page_allocation_t* alloc, page_range_t* range, page_allocation_t** plast, page_allocation_t** pfirst)
{
    page_allocation_t* last = nullptr;
    page_allocation_t* first = nullptr;

    // [ alloc ] [ range ]
    //         | |

    /* Calculate which start idx is the highest */
    const size_t highest_start_page_idx = MAX(alloc->range.page_idx, range->page_idx);
    /* Calculate which end idx is the lowest */
    const size_t lowest_end_page_idx = MIN(alloc->range.page_idx + (u64)alloc->range.nr_pages - 1, range->page_idx + (u64)range->nr_pages - 1);
    /* Now we can calculate how much of @range is inside @alloc */
    const size_t page_delta = lowest_end_page_idx < highest_start_page_idx
        ? 0
        : (lowest_end_page_idx - highest_start_page_idx) + 1;

    // KLOG_DBG("Comparing range 0x%llx(%d) to 0x%llx(%d)\n", alloc->range.page_idx, alloc->range.nr_pages, range->page_idx, range->nr_pages);

    /* If there is a page delta of 0, there is no part of @range inside @alloc */
    if (!page_delta)
        return -ENOENT;

    /* In this case we simply can't cut out a range =( */
    if (highest_start_page_idx != range->page_idx && __check_base_prepend(tracker, alloc, range))
        return -EINVAL;

    // KLOG_DBG("Delta: %lld\n", page_delta);
    // KLOG_DBG("Low end: %lld, High end: %lld\n", lowest_end_page_idx, highest_start_page_idx);

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
            .page_idx = range->page_idx + range->nr_pages,
            .nr_pages = (alloc->range.page_idx + alloc->range.nr_pages - 1) - lowest_end_page_idx,
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

        /* First set the fist to last_2, which may or may not be the first range */
        first = last_2;

        /* Cut off the alloc range */
        alloc->range.nr_pages -= (alloc_end_split.nr_pages + new_range.nr_pages);

        /* If @range sits at the front of @alloc, we need to remove alloc */
        if (highest_start_page_idx == alloc->range.page_idx || alloc->range.nr_pages == 0)
            __page_allocation_remove(tracker, alloc);
        else
            first = alloc;

        /* If the caller specified @pfirst, export our recorded first range */
        if (pfirst)
            *pfirst = first;
    }

update_range_and_exit:
    /* Update the ranges bounds */
    page_range_shrink_from_start(range, page_delta);

    return 0;
}

static inline bool __page_range_can_extend(page_range_t* first, page_range_t* last)
{
    // KLOG_DBG("Can merge? (%lld -> %lld, %lld + %lld), refs=(%lld, %lld), flags=(%lld, %lld)\n",
    // first->page_idx, last->page_idx, first->nr_pages, last->nr_pages,
    // first->refc, last->refc, first->flags, last->flags);
    /* Bordering ranges with the same number of references and the same flags may
     * merge */
    return ((first->page_idx + first->nr_pages) == last->page_idx && first->refc == last->refc && first->flags == last->flags);
}

static void __page_tracker_try_merge_ranges(page_tracker_t* tracker, page_allocation_t* start, page_allocation_t* end)
{
    page_allocation_t* next;

    for (page_allocation_t* alloc = start; alloc != nullptr; alloc = next) {
        next = alloc->nx_link;

        if (!next)
            break;

        /* Check if these ranges can be merged */
        if (!__page_range_can_extend(&alloc->range, &next->range))
            continue;

        /* Extend alloc by the amount of pages in next */
        alloc->range.nr_pages += next->range.nr_pages;

        /* We want to check this range again, so let's tell that to the for loop */
        next = alloc;

        /* Remove the next link */
        __page_allocation_remove(tracker, alloc->nx_link);
    }
}

static error_t __page_tracker_add_allocation(page_tracker_t* tracker, u64 page_idx, u64 page_count, u32 flags)
{
    error_t error;
    size_t target_size_pgs;
    page_range_t range;
    page_allocation_t* next;
    page_allocation_t* last;
    page_allocation_t* start = NULL;
    page_allocation_t* prev_start = NULL;

    /* Initialize a single range */
    init_page_range(&range, page_idx, page_count, flags, 1);

    /* If there are no ranges yet, we can just append this one at the base */
    if (!tracker->base)
        return __page_allocation_append_in_slot(tracker, &tracker->base, &range);

    /* Find the range closest to @page_idx */
    error = __tracker_get_closest_range(tracker, page_idx, &start, &prev_start);

    if (error)
        return error;

    /* Check which range we use as the start */
    if (page_idx < start->range.page_idx && prev_start)
        start = prev_start;

    /* Check if we can put this range (at least partially) in front of ->base */
    if (__check_base_prepend(tracker, tracker->base, &range))
        /* Check if that used up the range */
        if (!range.nr_pages)
            /* Yes. Dip */
            return 0;

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
        /* Pre-set last */
        last = a;

        /* First, check if @range is (partially) inside @a */
        (void)__cut_out_range_from_allocation(tracker, a, &range, &last, NULL);

        /* Spent the entire range, let's dip */
        if (!range.nr_pages)
            break;

        /* SKIP */
        if (next && next->range.page_idx == range.page_idx)
            continue;

        /* Quick assert for my sanity */
        ASSERT(!next || (next->range.page_idx > range.page_idx));

        target_size_pgs = MIN(
            range.nr_pages, (!next) ? (u32)0xffffffffUL : (next->range.page_idx - range.page_idx));

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
        page_range_shrink_from_start(&range, target_size_pgs);
    }

    return error;
}

static error_t __tracker_find_fitting_range(page_tracker_t* tracker, enum PAGE_TRACKER_ALLOC_MODE mode, u32 flags, size_t nr_pages, page_allocation_t*** palloc, page_range_t* prange)
{
    page_allocation_t **a, **last;
    u64 free_space_before;
    u64 prev_range_terminating_page = 0;

    /* TODO: Implement allocation modes */
    (void)mode;

    if (!tracker || !nr_pages || !prange)
        return -EINVAL;

    last = a = &tracker->base;

    /* Loop over the entire range */
    while (*a) {
        /* Compute how much space is in between this range and the previous */
        free_space_before = (*a)->range.page_idx - prev_range_terminating_page;

        /* Check if the free space is sufficient to fit our number of pages */
        if (free_space_before >= nr_pages) {
            /* Initialize a dummy range, spanning the entire free space */
            init_page_range(prange, prev_range_terminating_page, free_space_before, flags, 1);

            /* Maybe export the allocation */
            if (palloc)
                *palloc = last;

            /* Yeet out of here */
            return 0;
        }

        /* Compute the terminating page (the page after the ranges end-page) of the previous range */
        prev_range_terminating_page = ((*a)->range.page_idx + (*a)->range.nr_pages);

        /* Cache last */
        last = a;
        /* Set the next allocation */
        a = &(*a)->nx_link;
    }

    /* Shit, check if we have some space left on the far end */
    if (!(*a) && (tracker->max_page - prev_range_terminating_page) < nr_pages)
        return -ENOENT;

    /* Alright, the range is still able to fit after the last range. No big deal */
    init_page_range(prange, prev_range_terminating_page, (tracker->max_page - prev_range_terminating_page), flags, 1);

    /* Maybe export last allocation */
    if (palloc && last)
        *palloc = last;

    /* Dip */
    return 0;
}

/*!
 * @brief: Finds a free range that can contain @nr_pages
 */
error_t page_tracker_find_fitting_range(page_tracker_t* tracker, enum PAGE_TRACKER_ALLOC_MODE mode, size_t nr_pages, page_range_t* prange)
{
    error_t error;

    mutex_lock(&tracker->lock);

    error = __tracker_find_fitting_range(tracker, mode, NULL, nr_pages, NULL, prange);

    mutex_unlock(&tracker->lock);

    return error;
}

error_t page_tracker_alloc_any(page_tracker_t* tracker, enum PAGE_TRACKER_ALLOC_MODE mode, size_t nr_pages, u32 flags, page_range_t* prange)
{
    error_t error;
    page_allocation_t** last = nullptr;
    page_range_t target_range = { 0 };

    mutex_lock(&tracker->lock);

    /* Check error */
    error = __tracker_find_fitting_range(tracker, mode, flags, nr_pages, &last, &target_range);

    if (error || !last)
        goto unlock_and_exit;

    /* Reset the range, since __tracker_find_fitting_range maxes out the pagecount for the range it returns */
    target_range.nr_pages = nr_pages;

    /* Empty slot. We can just dump our range here */
    if (!(*last))
        __page_allocation_append_in_slot(tracker, last, &target_range);
    else {
        /* Check if we can extend the last range. Otherwise just append a new range */
        if (__page_range_can_extend(&(*last)->range, &target_range))
            (*last)->range.nr_pages += nr_pages;
        else
            /* Need to append AFTER this boi */
            __page_allocation_append(tracker, *last, &target_range);
    }

unlock_and_exit:
    mutex_unlock(&tracker->lock);

    if (IS_OK(error))
        memcpy(prange, &target_range, sizeof(target_range));

    return error;
}

error_t page_tracker_alloc(page_tracker_t* tracker, u64 start_page, size_t nr_pages, u32 flags)
{
    error_t error;

    mutex_lock(&tracker->lock);

    error = __page_tracker_add_allocation(tracker, start_page, nr_pages, flags);

    if (error)
        goto unlock_and_exit;

    /* Try to merge any werid ranges */
    __page_tracker_try_merge_ranges(tracker, tracker->base, nullptr);

unlock_and_exit:
    mutex_unlock(&tracker->lock);

    return error;
}

static inline bool __page_tracker_try_deref_range(page_tracker_t* tracker, page_allocation_t* allocation, page_range_t* dummy_range, page_range_t* range)
{
    // KLOG_DBG("Checking range a: %lld(%d), b: %lld(%d)\n", allocation->range.page_idx, allocation->range.nr_pages, dummy_range->page_idx, dummy_range->nr_pages);
    if (!page_range_equal_bounds(&allocation->range, dummy_range))
        return false;

    /* Remove the needed references from the actual allocation */
    if (__page_allocation_deref(tracker, allocation, range->refc))
        return false;

    /* Shrink the actual range by the size of the dummy range we created */
    page_range_shrink_from_start(range, dummy_range->nr_pages);

    return true;
}

error_t page_tracker_dealloc(page_tracker_t* tracker, page_range_t* range)
{
    error_t error;
    size_t distance_to_next_range;
    page_range_t internal_range = { 0 };
    page_range_t dummy_range;
    page_allocation_t* first = NULL;
    page_allocation_t* start = NULL;
    page_allocation_t* next;

    if (!tracker || !range)
        return -EINVAL;

    if (!tracker->base)
        return -EINVAL;

    mutex_lock(&tracker->lock);

    /* Grab the closest range to @range */
    error = __tracker_get_closest_range(tracker, range->page_idx, &start, NULL);

    if (error || !start)
        goto unlock_and_exit;

    // KLOG_DBG("Closest range to r:%llx(%d) is c:%llx(%d)\n", range->page_idx, range->nr_pages, start->range.page_idx, start->range.nr_pages);

    // if (start->nx_link) {
    // KLOG_DBG("   ==> Next: %llx(%d)\n", start->nx_link->range.page_idx, start->nx_link->range.nr_pages);
    //}

    /*
     * If @range lies outside the starting range for a part, we need to simply shave off that
     * extra space, since we don't have any allocations there. If we need to do this, that
     * (most likely) means that the caller passed an invalid range...
     * TODO: Register these shrinkages?
     */
    if (page_range_shrink_from_start(range, page_range_get_distance_to_page(&start->range, range->page_idx)))
        goto unlock_and_exit;

    /* Loop over all the allocations that contain @range */
    for (page_allocation_t* alloc = start; alloc != nullptr; alloc = next) {
        next = alloc->nx_link;

        /* Calculate how far the start of @range is from this allocation */
        distance_to_next_range = page_range_get_distance_to_page(&alloc->range, range->page_idx);

        /* Shrink the range if it isn't in this range */
        if (page_range_shrink_from_start(range, distance_to_next_range))
            break;

        /* We know @range is (for a part) inside @alloc. Let's calculate how much of it is in there */
        first = NULL;

        /* Initialize this range */
        init_page_range(
            &internal_range,
            range->page_idx,
            page_range_get_pages_inside(range, &alloc->range),
            alloc->range.flags,
            0);

        /* Make a copy of @internal_range, to make sure __cut_out_range_from_allocation leaves it alone */
        dummy_range = internal_range;

        /* If we where able to simply dereference this range, we can go next */
        if (__page_tracker_try_deref_range(tracker, alloc, &internal_range, range))
            continue;

        /* @internal_range is only inside a part of @alloc. We now need to cut it out of @alloc =( */
        error = __cut_out_range_from_allocation(tracker, alloc, &dummy_range, NULL, &first);

        /*
         * This would be extremely bad, since we might have damaged the tracker at this point.
         * This is why, if any tracker call fails with anything other than -ENOMEM, the entire
         * kernel should halt pretty much =(
         */
        if (error || !first)
            goto unlock_and_exit;

        /* Remove the references from the range we just cut out */
        for (page_allocation_t* inner_alloc = first; inner_alloc != next; inner_alloc = inner_alloc->nx_link)
            /* Find the range we just cut out */
            if (__page_tracker_try_deref_range(tracker, inner_alloc, &internal_range, range))
                break;
    }

    /* Check the entire range for mergeables */
    __page_tracker_try_merge_ranges(tracker, tracker->base, nullptr);

unlock_and_exit:
    mutex_unlock(&tracker->lock);

    return error;
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

/*!
 * @brief: Grab the range that includes @page_idx. Returns an error if there isn't one
 */
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

/*!
 * @brief: Copy the ranges inside @src into a new empty page tracker at @dest
 */
error_t page_tracker_copy(page_tracker_t* src, page_tracker_t* dest)
{
    kernel_panic("TODO: page_tracker_copy");
    return 0;
}

/*!
 * @brief: Cound the amount of pages used inside @tracker
 */
error_t page_tracker_get_used_page_count(page_tracker_t* tracker, size_t* pcount)
{
    size_t total_sz = 0;

    if (!tracker || !pcount)
        return -EINVAL;

    mutex_lock(&tracker->lock);

    /* Loop over all allocations to sum up all the pages */
    for (page_allocation_t* allocation = tracker->base; allocation; allocation = allocation->nx_link)
        total_sz += allocation->range.nr_pages;

    mutex_unlock(&tracker->lock);

    *pcount = total_sz;

    return 0;
}
