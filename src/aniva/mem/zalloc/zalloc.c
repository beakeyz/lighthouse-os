#include "zalloc.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/zalloc/list.h"
#include "sys/types.h"
#include <mem/heap.h>
#include <mem/kmem.h>

#include <tests/tests.h>

/* Only the sized list needs to know what its end is */

zalloc_list_t* __kernel_alloc_list;

zone_allocator_t __initial_allocator[DEFAULT_ZONE_ENTRY_SIZE_COUNT];

static inline void* zone_allocate(zone_t* zone, size_t size);
static inline int zone_deallocate(zone_t* zone, void* address, size_t size);

void init_zalloc()
{
    __kernel_alloc_list = create_zalloc_list(1);
}

#define DEFAULT_ZONE_STORE_CAPACITY ((SMALL_PAGE_SIZE - sizeof(zone_store_t)) >> 3)

/*
 * This is the minimal size a buffer must have in order to support a complete
 * zone allocator.
 */
#define __MIN_INITIAL_SIZE \
    ALIGN_UP(sizeof(zone_store_t) + (DEFAULT_ZONE_STORE_CAPACITY * sizeof(zone_t*)) + sizeof(zone_t), SMALL_PAGE_SIZE)

int init_zone_allocator(zone_allocator_t* ret, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags)
{
    int error = -1;
    size_t entries_for_this_zone;
    zone_t* new_zone;
    zone_store_t* new_zone_store;

    if (!ret || !initial_size || !hard_max_entry_size)
        return -KERR_INVAL;

    new_zone = nullptr;
    new_zone_store = nullptr;

    // Initialize the initial zones
    ret->m_total_size = 0;
    ret->m_grow_size = initial_size;

    new_zone_store = create_zone_store(DEFAULT_ZONE_STORE_CAPACITY);

    if (!new_zone_store)
        goto fail_and_dealloc;

    // Create a store that can hold the maximum amount of zones for 1 page
    ret->m_store = new_zone_store;
    ret->m_store_count = 1;
    ret->m_flags = flags;

    ret->m_entry_size = hard_max_entry_size;

    /* FIXME: integer devision */
    /* Calculate how big this zone needs to be */
    entries_for_this_zone = (initial_size / hard_max_entry_size);

    new_zone = create_zone(ret, hard_max_entry_size, entries_for_this_zone);

    if (!new_zone)
        goto fail_and_dealloc;

    error = zone_store_add(ret->m_store, new_zone);

    if (error)
        goto fail_and_dealloc;

    ret->m_total_size += new_zone->m_total_available_size;

    return 0;

fail_and_dealloc:

    if (new_zone)
        destroy_zone(ret, new_zone);

    if (new_zone_store)
        destroy_zone_store(ret, new_zone_store);

    kfree(ret);
    return error;
}

zone_allocator_t* create_zone_allocator(size_t initial_size, size_t hard_max_entry_size, uintptr_t flags)
{
    return create_zone_allocator_ex(nullptr, 0, initial_size, hard_max_entry_size, flags);
}

zone_allocator_t* create_zone_allocator_ex(pml_entry_t* map, vaddr_t start_addr, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags)
{
    zone_allocator_t* ret;

    if (!initial_size || !hard_max_entry_size)
        return nullptr;

    /* FIXME: as an allocator, we don't want to depend on another allocator to be created */
    ret = kmalloc(sizeof(zone_allocator_t));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    if (init_zone_allocator(ret, initial_size, hard_max_entry_size, flags))
        return nullptr;

    return ret;
}

int init_zone_allocator_ex(zone_allocator_t* ret, void* start_buffer, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags)
{
    int error = -1;
    void* c_ptr;
    size_t c_size;
    zone_t* new_zone;
    zone_store_t* new_zone_store;

    if (!ret || !initial_size || !hard_max_entry_size)
        return -KERR_INVAL;

    if (initial_size < __MIN_INITIAL_SIZE)
        return -EBADSIZE;

    new_zone = nullptr;
    new_zone_store = nullptr;
    c_ptr = start_buffer;

    // Initialize the initial zones
    ret->m_total_size = 0;
    ret->m_flags = flags;
    ret->m_grow_size = initial_size;
    ret->m_entry_size = hard_max_entry_size;

    /* Calculate the current size of the init buffer */
    c_size = sizeof(zone_store_t) + sizeof(zone_t*) * DEFAULT_ZONE_STORE_CAPACITY;
    /* Initialize the new zone store */
    new_zone_store = init_zone_store(c_ptr, c_size);

    /* Check if we've died */
    if (!new_zone_store)
        return -ENULL;

    /* Add a bit to the current pointer */
    c_ptr += c_size;

    // Create a store that can hold the maximum amount of zones for 1 page
    ret->m_store = new_zone_store;
    ret->m_store_count = 1;

    /* Calculate what the remaining space is */
    c_size = initial_size - c_size;

    /* Initialize a zone at the remaining space */
    new_zone = init_zone(ret, c_ptr, c_size, hard_max_entry_size);

    /* Check if we've fucking died */
    if (!new_zone)
        return -ENULL;

    /* Add to the boi */
    error = zone_store_add(ret->m_store, new_zone);

    if (error)
        return -ENULL;

    /* Set how much stuff we have now */
    ret->m_total_size += new_zone->m_total_available_size;

    return 0;
}

static error_t __test_init_zalloc_ex(aniva_test_t* test)
{
    error_t error;
    zone_allocator_t zallocator;
    char buffer[__MIN_INITIAL_SIZE];

    /* Try to initialize deze shit */
    error = init_zone_allocator_ex(&zallocator, buffer, sizeof(buffer), 32, NULL);

    if (error) {
        KLOG("error: %d ", error);
        return error;
    }

    /* Allocate a test string */
    char* a = zalloc_fixed(&zallocator);

    /* Check if we recieved a valid pointer */
    if (!a || a < buffer || a >= (buffer + sizeof(buffer)))
        return -EINVAL;

    /* Do another allocation */
    char* b = zalloc_fixed(&zallocator);

    /* Check if we recieved a valid pointer */
    if (!b || b < buffer || b >= (buffer + sizeof(buffer)))
        return -EINVAL;

    /* Free this shit */
    zfree_fixed(&zallocator, a);

    /* Check if the bitmaps reflect the correct values */
    if (bitmap_isset(&zallocator.m_store->m_zones[0]->m_entries, 0) || !bitmap_isset(&zallocator.m_store->m_zones[0]->m_entries, 1))
        return -EINVAL;

    return 0;
}

ANIVA_REGISTER_TEST("init zone allocator", init_zone_allocator_ex, __test_init_zalloc_ex, ANIVA_TEST_TYPE_MEM);

void destroy_zone_allocator(zone_allocator_t* allocator, bool clear_zones)
{
    (void)clear_zones;

    // detach_allocator(allocator);

    destroy_zone_stores(allocator);

    // destroy_heap(allocator->m_heap);

    kfree(allocator);
}

/*!
 * @brief: Clear all the allocations inside the zone allocator
 *
 * NOTE: this does not erase the data inside the zones, so calling zalloc always
 * requires the caller to do a memset
 */
void zone_allocator_clear(zone_allocator_t* allocator)
{
    zone_store_t* c_store;

    c_store = allocator->m_store;

    /* Loop over all the stores in this allocator */
    while (c_store) {

        /* Clear all the zones inside this store */
        for (uint32_t i = 0; i < c_store->m_zones_count; i++)
            bitmap_unmark_range(&c_store->m_zones[i]->m_entries, 0, c_store->m_zones[i]->m_entries.m_entries);

        c_store = c_store->m_next;
    }
}

/*!
 * @brief Try to add zones to the allocator
 *
 * We try really hard to add a new zone to this allocator, by either adding a new zone
 * to an exsisting store, or otherwise creating a new store to put the new zone into
 * @returns a pointer to the zone that was added to the allocator
 */
static int grow_zone_allocator(zone_allocator_t* allocator, zone_t** p_zone)
{

    zone_t* new_zone;
    zone_store_t* new_zone_store;
    size_t grow_size;
    size_t entry_size;
    int error;

    if (!allocator)
        return -1;

    /*
     * Create a new zone to fit the new size
     */

    grow_size = allocator->m_grow_size;
    entry_size = allocator->m_entry_size;

    /* FIXME: integer devision */
    size_t entries_for_this_zone = (grow_size / entry_size);

    new_zone = create_zone(allocator, allocator->m_entry_size, entries_for_this_zone);

    /* Failed to create zone: probably out of physical memory */
    if (!new_zone)
        return -1;

    error = allocator_add_zone(allocator, new_zone);

    /*
     * If we where able to add the new zone, thats a successful growth.
     * Otherwise we need to add a new zone store...
     */
    if (!error) {
        *p_zone = new_zone;
        return 0;
    }

    new_zone_store = create_zone_store(DEFAULT_ZONE_STORE_CAPACITY);

    if (!new_zone_store)
        return -KERR_NOMEM;

    error = zone_store_add(new_zone_store, new_zone);

    /* Could not even add a zone to the new store we just made.. this is bad =( */
    if (error)
        return error;

    /* Yay we have a new store that also has a clean zone! lets add it to the allocator */

    new_zone_store->m_next = allocator->m_store;

    allocator->m_store = new_zone_store;
    allocator->m_store_count++;

    *p_zone = new_zone;
    return 0;
}

zone_store_t* init_zone_store(void* buffer, size_t bsize)
{
    zone_store_t* ret;

    if (!buffer || !bsize)
        return nullptr;

    /* Clear the buffer */
    memset(buffer, 0, bsize);

    ret = buffer;

    ret->m_next = nullptr;
    ret->m_zones_count = 0;
    ret->m_capacity = ALIGN_DOWN(bsize - sizeof(zone_store_t), sizeof(zone_t*));

    return ret;
}

zone_store_t* create_zone_store(size_t initial_capacity)
{
    int error;
    zone_store_t* store;

    // We subtract the size of the entire zone_store header struct without
    // the size of the m_zones field
    const size_t bytes = ALIGN_UP(initial_capacity * sizeof(zone_t*) + (sizeof(zone_store_t)), SMALL_PAGE_SIZE);
    /* The amount of bytes that we gain by aligning up */
    const size_t delta_bytes = (bytes - sizeof(zone_store_t)) - (initial_capacity * sizeof(zone_t*));
    const size_t delta_entries = delta_bytes ? (delta_bytes >> 3) : 0; // delta / sizeof(zone_t*)

    initial_capacity += delta_entries;

    error = kmem_kernel_alloc_range((void**)&store, bytes, 0, KMEM_FLAG_WRITABLE);

    if (error)
        return nullptr;

    // set the counts
    store->m_zones_count = 0;
    store->m_next = nullptr;
    store->m_capacity = initial_capacity;

    // Make sure this zone store is uninitialised
    memset(store->m_zones, 0, initial_capacity * sizeof(zone_t*));
    return store;
}

void destroy_zone_store(zone_allocator_t* allocator, zone_store_t* store)
{

    for (uintptr_t i = 0; i < store->m_zones_count; i++) {
        zone_t* zone = store->m_zones[i];

        /* Just to be sure yk */
        if (!zone)
            continue;

        destroy_zone(allocator, zone);
    }

    kmem_kernel_dealloc((uintptr_t)store, store->m_capacity * sizeof(uintptr_t) + (sizeof(zone_store_t) - 8));
}

void destroy_zone_stores(zone_allocator_t* allocator)
{

    zone_store_t* current;

    if (!allocator)
        return;

    while (allocator->m_store) {

        /* Cache pointer to the current allocator */
        current = allocator->m_store;
        /* Cycle early */
        allocator->m_store = allocator->m_store->m_next;

        /* The current zone is now removed from the link, so we can kill it */
        destroy_zone_store(allocator, current);
    }
}

int allocator_add_zone(zone_allocator_t* allocator, zone_t* zone)
{
    int error;

    if (!allocator || !zone)
        return -1;

    FOREACH_ZONESTORE(allocator, store)
    {
        error = zone_store_add(store, zone);

        if (!error)
            return error;
    }

    return error;
}

int allocator_remove_zone(zone_allocator_t* allocator, zone_t* zone)
{
    int error;

    if (!allocator || !zone)
        return -1;

    FOREACH_ZONESTORE(allocator, store)
    {
        error = zone_store_remove(store, zone);

        if (!error)
            return error;
    }

    return error;
}

int zone_store_add(zone_store_t* store, zone_t* zone)
{
    if (!store || !zone || !store->m_capacity)
        return -1;

    // More zones than we can handle?
    if (store->m_zones_count > store->m_capacity)
        return -1;

    store->m_zones[store->m_zones_count] = zone;

    store->m_zones_count++;

    return 0;
}

/*
 * We assume the zone is already destroyed when we remove it
 */
int zone_store_remove(zone_store_t* store, zone_t* zone)
{

    if (!store || !zone)
        return -1;

    // Sanity check
    if (store->m_zones_count == (uintptr_t)-1) {
        kernel_panic("FUCK: store->m_zones_count seems to be invalid?");
    }

    // Find the index of our entry
    uintptr_t index = (uintptr_t)-1;

    for (uintptr_t i = 0; i < store->m_zones_count; i++) {
        if (store->m_zones[i] == zone) {
            index = i;
            break;
        }
    }

    // Let's assume we never get to this point lmao
    if (index == (uintptr_t)-1)
        return -1;

    // Shift every entry after the one we want to remove back by one
    for (uintptr_t i = index; i < store->m_zones_count - 1; i++) {
        store->m_zones[i] = store->m_zones[i + 1];
    }

    store->m_zones_count--;

    return 0;
}

void destroy_zone(zone_allocator_t* allocator, zone_t* zone)
{
    /* TODO: use this to grab the pml root */
    (void)allocator;

    size_t zone_size = sizeof(zone_t);

    zone_size += zone->m_entries.m_size;
    zone_size += zone->m_total_available_size;

    /* TODO: resolve pml root */
    kmem_dealloc(nullptr, nullptr, (vaddr_t)zone, zone_size);
}

static inline size_t __calculate_optimal_nr_bitmap_entries(size_t bsize, const size_t entry_size, size_t max_itter)
{
    size_t nr_entries = 0;
    size_t prev_nr_entries = 0;
    size_t bitmap_sz = 0;

    do {
        /* Store the previous */
        prev_nr_entries = nr_entries;
        /* Calculate the current count */
        nr_entries = ALIGN_DOWN((bsize - bitmap_sz) / entry_size, 8);

        /* Calculate the bitmap size */
        bitmap_sz = BITS_TO_BYTES(nr_entries);

        /*
         * Continue the itteration until we can't get more persise, or
         * until we've reached the maximum number of itterations
         */
    } while (prev_nr_entries != nr_entries && (max_itter--));

    return nr_entries;
}

static error_t __test_calc_optimal_nr_bitmap_entries(aniva_test_t* test)
{
    const size_t test_sz = 0x4000;
    const size_t entry_sz = 32;
    const size_t max_iter = 32;

    const size_t expected_result = 504;
    const size_t result = __calculate_optimal_nr_bitmap_entries(test_sz, entry_sz, max_iter);

    if (result != expected_result) {
        KLOG("Got: %lld, Expected: %lld ", result, expected_result);
        return -EINVAL;
    }

    return 0;
}

/* TODO: Handle tests */
ANIVA_REGISTER_TEST("Calc optimal nr_bitmap_entries", __calculate_optimal_nr_bitmap_entries, __test_calc_optimal_nr_bitmap_entries, ANIVA_TEST_TYPE_ALGO);

/*!
 * @brief: Tries to initialize a zone inside a pre-allocated buffer
 */
zone_t* init_zone(zone_allocator_t* allocator, void* buffer, size_t bsize, const size_t entry_size)
{
    zone_t* zone;

    /* No buffer, no zone =( */
    if (!buffer)
        return nullptr;

    /* Asser this shit */
    ASSERT_MSG(entry_size != 0 && bsize != 0, "create_zone: expected non-null parameters!");

    /* The zone struct will live at the start of the supplied buffer */
    zone = buffer;

    /* Calculate how many entries our bitmap will have */
    const size_t nr_bitmap_entries = __calculate_optimal_nr_bitmap_entries(ALIGN_DOWN((bsize - sizeof(zone_t)), 8), entry_size, 32);

    /* Construct the bitmap */
    init_bitmap(&zone->m_entries, nr_bitmap_entries, BITS_TO_BYTES(nr_bitmap_entries), 0);

    /* Set the size of the entries we allocate */
    zone->m_zone_entry_size = entry_size;
    /* Calculate where the memory actually starts */
    zone->m_total_available_size = zone->m_entries.m_entries * zone->m_zone_entry_size;
    /* Initialize the zone data fields */
    zone->m_entries_start = ((uintptr_t)zone + sizeof(zone_t) + BITS_TO_BYTES(nr_bitmap_entries));

    return zone;
}

/*
 * TODO: we can add some random padding between the zone_t struct
 * and where the actual memory storage starts
 */
zone_t* create_zone(zone_allocator_t* allocator, const size_t entry_size, size_t max_entries)
{
    int error;
    zone_t* zone;

    ASSERT_MSG(entry_size != 0 && max_entries != 0, "create_zone: expected non-null parameters!");

    uint32_t kmem_flags = KMEM_FLAG_WRITABLE;
    size_t bitmap_bytes = BITS_TO_BYTES(max_entries);
    size_t total_entries_bytes = entry_size * max_entries;

    const size_t bytes = sizeof(zone_t) + bitmap_bytes + total_entries_bytes;
    const size_t aligned_size = ALIGN_UP(bytes, SMALL_PAGE_SIZE);

    // The amount of bytes we got for free as a result of the alignment
    size_t size_delta = aligned_size - bytes;

    // How many extra entries will that get us?
    size_t extra_entries = size_delta / entry_size;

    // How many extra bytes do we need in our bitmap then?
    size_t extra_bitmap_bytes = BITS_TO_BYTES(extra_entries);

    size_t previous_extra_bytes;

    // Let's just shave off those bytes as long as there is still room left
    for (;;) {
        previous_extra_bytes = extra_bitmap_bytes;

        size_delta -= extra_bitmap_bytes;

        extra_entries = size_delta / entry_size;

        extra_bitmap_bytes = BITS_TO_BYTES(extra_entries);

        /* Check if we have reached a maximum */
        if (previous_extra_bytes == extra_bitmap_bytes)
            break;
    }

    max_entries += extra_entries;
    bitmap_bytes += extra_bitmap_bytes;

    /* Make sure the zone has the correct memory flags */
    if ((allocator->m_flags & ZALLOC_FLAG_DMA) == ZALLOC_FLAG_DMA)
        kmem_flags |= KMEM_FLAG_DMA;

    error = kmem_kernel_alloc_range((void**)&zone, aligned_size, 0, kmem_flags);

    if (error)
        return nullptr;

    /* Construct the bitmap */
    init_bitmap(&zone->m_entries, max_entries, bitmap_bytes, 0);

    /* Set the size of the entries we allocate */
    zone->m_zone_entry_size = entry_size;

    // Calculate where the memory actually starts
    const size_t entries_bytes = zone->m_entries.m_entries * zone->m_zone_entry_size;

    zone->m_entries_start = ((uintptr_t)zone + aligned_size) - entries_bytes;
    zone->m_total_available_size = entries_bytes;

    return zone;
}

void* kzalloc(size_t size)
{
    return zalloc_listed(__kernel_alloc_list, size);
}

void kzfree(void* address, size_t size)
{
    zfree_listed(__kernel_alloc_list, address, size);
}

void kzfree_scan(void* address)
{
    zfree_listed_scan(__kernel_alloc_list, address);
}

void* zalloc(zone_allocator_t* allocator, size_t size)
{
    int error;
    void* result;
    zone_t* new_zone;
    zone_store_t* current_store;

    if (!allocator || !size)
        return nullptr;

    current_store = allocator->m_store;

    while (current_store) {

        for (uintptr_t i = 0; i < current_store->m_zones_count; i++) {
            zone_t* zone = current_store->m_zones[i];

            if (!zone)
                continue;

            if (size <= zone->m_zone_entry_size) {
                result = zone_allocate(zone, size);

                if (!result)
                    continue;

                return result;
            }
        }

        current_store = current_store->m_next;
    }

    error = grow_zone_allocator(allocator, &new_zone);

    if (error)
        return nullptr;

    result = zone_allocate(new_zone, size);

    if (result)
        return result;

    /* TODO: try to create a new zone somewhere and allocate there */
    kernel_panic("zalloc call failed! TODO: fix this dumb panic!");

    return nullptr;
}

void zfree(zone_allocator_t* allocator, void* address, size_t size)
{
    int error;
    zone_store_t* current_store;

    if (!allocator || !address || !size)
        return;

    current_store = allocator->m_store;

    while (current_store) {

        for (uintptr_t i = 0; i < current_store->m_zones_count; i++) {
            zone_t* zone = current_store->m_zones[i];

            if (!zone)
                continue;

            if (size <= zone->m_zone_entry_size) {
                error = zone_deallocate(zone, address, size);

                if (error)
                    continue;

                // If the deallocation succeeded, just exit
                break;
            }
        }

        current_store = current_store->m_next;
    }
}

void* zalloc_fixed(zone_allocator_t* allocator)
{
    return zalloc(allocator, allocator->m_entry_size);
}

void zfree_fixed(zone_allocator_t* allocator, void* address)
{
    zfree(allocator, address, allocator->m_entry_size);
}

static inline void* zone_allocate(zone_t* zone, size_t size)
{
    uintptr_t index;

    // ASSERT_MSG(size <= zone->m_zone_entry_size, "Allocating over the span of multiple subzones is not yet implemented!");

    // const size_t entries_needed = (size + zone->m_zone_entry_size - 1) / zone->m_zone_entry_size;
    if (!KERR_OK(bitmap_find_free(&zone->m_entries, &index)))
        return nullptr;

    bitmap_mark(&zone->m_entries, index);

    return (void*)(zone->m_entries_start + (index * zone->m_zone_entry_size));
}

static inline int zone_deallocate(zone_t* zone, void* address, size_t size)
{
    ASSERT_MSG(size <= zone->m_zone_entry_size, "Deallocating over the span of multiple subzones is not yet implemented!");

    // Address is not contained inside this zone
    if ((uintptr_t)address < zone->m_entries_start)
        return -1;

    /*
     * FIXME: this integer division is dangerous. We really should only accept sizes that are
     * a power of two and if they are not, we should round them up to be that size. This way we can
     * store the log2 of the entry_size which will make sure that we can ALWAYS find back the address
     * of any allocation
     */
    uintptr_t address_offset = (uintptr_t)(address - zone->m_entries_start) / zone->m_zone_entry_size;

    // If an offset is equal to or greater than the amount of entries, something has
    // gone wrong, since offsets start at 0
    if (address_offset >= zone->m_entries.m_entries)
        return -1;

    if (!bitmap_isset(&zone->m_entries, address_offset))
        return -1;

    bitmap_unmark(&zone->m_entries, address_offset);

    return 0;
}
