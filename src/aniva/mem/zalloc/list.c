#include "list.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"

static const enum ZONE_ENTRY_SIZE __default_entry_sizes[DEFAULT_ZONE_ENTRY_SIZE_COUNT] = {
    [0] = ZALLOC_8BYTES,
    [1] = ZALLOC_16BYTES,
    [2] = ZALLOC_32BYTES,
    [3] = ZALLOC_64BYTES,
    [4] = ZALLOC_128BYTES,
    [5] = ZALLOC_256BYTES,
    [6] = ZALLOC_512BYTES,
    [7] = ZALLOC_1024BYTES,
};

/*
 * This could probably also be done with a binary search
 */
static kerror_t __allocator_list_add_allocator(zalloc_list_t* list, zone_allocator_t* allocator)
{
    size_t size_to_add;
    zone_allocator_t* next;
    zone_allocator_t** itt;

    if (!list || !allocator || !allocator->m_entry_size)
        return -1;

    size_to_add = allocator->m_entry_size;

    for (itt = &list->list; *itt; itt = &(*itt)->m_next) {
        next = (*itt)->m_next;

        /* No duplicate sizes */
        if (size_to_add == (*itt)->m_entry_size)
            return -1;

        if (size_to_add > (*itt)->m_entry_size && (!next || size_to_add < next->m_entry_size))
            break;
    }

    if (*itt) {

        /*
         * Since the size of the allocator to add is inbetween the itterator and the nexts size,
         * we need to put it inbetween the two
         */
        allocator->m_next = next;
        (*itt)->m_next = allocator;

    } else
        *itt = allocator;

    list->allocator_count++;
    return (0);
}

/*
 * Find the middle of a range using a fast and a slow pointer
 */
static zone_allocator_t** __try_find_middle_zone_allocator(zone_allocator_t** start, zone_allocator_t** end)
{

    zone_allocator_t** slow = start;
    zone_allocator_t** fast = start;

    while (*fast && ((*fast)->m_next) && (fast != end && (&(*fast)->m_next != end))) {
        slow = &(*slow)->m_next;
        fast = &(*fast)->m_next->m_next;
    }

    return slow;
}

/*!
 * @brief Find the optimal zone allocator to store a allocatin of the size specified
 *
 * Try to find a zone allocator that fits our size specification. The
 */
static zone_allocator_t* __get_allocator_for_size(zalloc_list_t* list, size_t size)
{
    zone_allocator_t** current;
    zone_allocator_t** priv = nullptr;
    zone_allocator_t** end = nullptr;
    zone_allocator_t** start = &list->list;

    while ((current = __try_find_middle_zone_allocator(start, end)) != nullptr) {
        if (!current || !(*current) || current == priv)
            break;

        /* If we accept this allocation size, break */
        if ((*current)->m_entry_size == size || ((*current)->m_entry_size - size) <= ZALLOC_ACCEPTABLE_MEMSIZE_DEVIATON)
            return (*current);

        /*
         * Otherwise devide the list and search the half that must contain the correct size
         */
        if ((*current)->m_entry_size > size) {
            end = current;
        } else {
            start = current;
        }

        priv = current;
    }

    return nullptr;
}

void* zalloc_listed(zalloc_list_t* list, size_t size)
{
    size_t allocator_size;
    zone_allocator_t* allocator;

    if (!size)
        return nullptr;

    allocator = __get_allocator_for_size(list, size);

    if (!allocator) {

        allocator_size = ZALLOC_DEFAULT_ALLOC_COUNT * size;

        /* Let's try to create a new allocator for this size */
        allocator = create_zone_allocator(allocator_size, size, ZALLOC_FLAG_KERNEL);

        ASSERT(!__allocator_list_add_allocator(list, allocator));
    }

    /*
    if (!allocator)
      return nullptr;
      */

    ASSERT_MSG(allocator, "Failed to find zone allocator for kernel allocation!");

    return zalloc(allocator, size);
}

void zfree_listed(zalloc_list_t* list, void* address, size_t size)
{
    zone_allocator_t* allocator;

    if (!size)
        return;

    allocator = __get_allocator_for_size(list, size);

    /*
    if (!allocator)
      return;
      */

    ASSERT_MSG(allocator, "Failed to find zone allocator to free kernel allocation!");

    zfree(allocator, address, size);
}

/*!
 * @brief: Free an address if we don't know the entry size
 *
 * Can be quite slowe, so
 * TODO: Optimize by caching recent allocations made in an allocator list so we
 * don't have to do expensive lookups every time there is a free call
 */
kerror_t zfree_listed_scan(zalloc_list_t* list, void* address)
{
    zone_t* c_zone;
    zone_store_t* c_store;
    zone_allocator_t* alloc;

    /* Scan over all the allocators to find which one containse @address */
    for (alloc = list->list; alloc; alloc = alloc->m_next) {
        c_store = alloc->m_store;

        while (c_store) {

            for (uint32_t i = 0; i < c_store->m_zones_count; i++) {
                c_zone = c_store->m_zones[i];

                if (c_zone->m_entries_start < (vaddr_t)address && (c_zone->m_entries_start + c_zone->m_total_available_size) >= (vaddr_t)address)
                    break;
            }

            c_zone = nullptr;
            c_store = c_store->m_next;
        }
    }

    if (!c_zone || !c_store || !alloc)
        return -1;

    zfree_fixed(alloc, address);
    return 0;
}

/*!
 * @brief: Create a list of allocators
 */
zalloc_list_t* create_zalloc_list(uint32_t pagecount)
{
    zalloc_list_t* list;
    zone_allocator_t* current;

    ASSERT_MSG(pagecount < 4, "Tried to create a zalloc list that's too large (max 4 pages)");

    ASSERT(!kmem_kernel_alloc_range((void**)&list, pagecount << PAGE_SHIFT, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));
    ASSERT_MSG(list, "Failed to allocate zalloc list");

    /* Clear it */
    memset(list, 0, pagecount << PAGE_SHIFT);

    list->this_pagecount = pagecount;

    for (uintptr_t i = 0; i < DEFAULT_ZONE_ENTRY_SIZE_COUNT; i++) {

        current = create_zone_allocator(16 * Kib, __default_entry_sizes[i], NULL);

        ASSERT_MSG(current, "Failed to create kernel zone allocators");

        ASSERT(!__allocator_list_add_allocator(list, current));
    }

    return list;
}

/*!
 * @brief: Murder a list of allocators
 */
void destroy_zalloc_list(zalloc_list_t* list)
{
    zone_allocator_t *walker, *next;

    ASSERT_MSG(list->list, "Tried to destroy a zalloc_list with no list lmao");

    walker = list->list;

    do {
        next = walker->m_next;

        destroy_zone_allocator(walker, true);

        walker = next;
    } while (walker);

    kmem_kernel_dealloc((vaddr_t)list, list->this_pagecount << PAGE_SHIFT);
}
