#include "zalloc.h"
#include "heap.h"
#include "kmem_manager.h"
#include "dev/debug/serial.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "libk/string.h"

/* Only the sized list needs to know what its end is */

size_t __kernel_allocator_count;
zone_allocator_t* __kernel_allocators;

zone_allocator_t __initial_allocator[DEFAULT_ZONE_ENTRY_SIZE_COUNT];

static ErrorOrPtr zone_allocate(zone_t* zone, size_t size);
static ErrorOrPtr zone_deallocate(zone_t* zone, void* address, size_t size);

static const enum ZONE_ENTRY_SIZE __default_entry_sizes[DEFAULT_ZONE_ENTRY_SIZE_COUNT] = {
  [0] = ZALLOC_8BYTES,
  [1] = ZALLOC_16BYTES,
  [2] = ZALLOC_32BYTES,
  [3] = ZALLOC_64BYTES,
  [4] = ZALLOC_128BYTES,
  //
  [5] = ZALLOC_256BYTES,
  [6] = ZALLOC_512BYTES,
  [7] = ZALLOC_1024BYTES,
};

/*
 * Find the middle of a range using a fast and a slow pointer
 */
static zone_allocator_t** __try_find_middle_zone_allocator(zone_allocator_t** start, zone_allocator_t** end) {

  zone_allocator_t** slow = start;
  zone_allocator_t** fast = start;

  while (*fast && ((*fast)->m_next) && (fast != end && (&(*fast)->m_next != end))) {
    slow = &(*slow)->m_next;
    fast = &(*fast)->m_next->m_next;
  }

  return slow;
}

/*
 * This could probably also be done with a binary search
 */
static ErrorOrPtr __kallocators_add_allocator(zone_allocator_t* allocator)
{
  size_t size_to_add;
  zone_allocator_t* next;
  zone_allocator_t** itt;

  if (!allocator || !allocator->m_entry_size)
    return Error();

  size_to_add = allocator->m_entry_size;

  for (itt = &__kernel_allocators; *itt; itt = &(*itt)->m_next) {
     next = (*itt)->m_next;

    /* No duplicate sizes */
    if (size_to_add == (*itt)->m_entry_size)
      return Error();

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

  } else {
    *itt = allocator;
  }

  __kernel_allocator_count++;

  return Success(0);
}

/*!
 * @brief Find the optimal zone allocator to store a allocatin of the size specified
 *
 * Try to find a zone allocator that fits our size specification. The 
 */
static zone_allocator_t* __get_allocator_for_size(size_t size)
{
  zone_allocator_t** current;
  zone_allocator_t** priv = nullptr; 

  zone_allocator_t** end = nullptr; 
  zone_allocator_t** start = &__kernel_allocators; 

  while ((current = __try_find_middle_zone_allocator(start, end)) != nullptr) {
    if (!current || !(*current) || current == priv)
      break;

    println(to_string((*current)->m_entry_size));

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

void init_zalloc() {
  __kernel_allocator_count = 0;
  __kernel_allocators = nullptr;

  zone_allocator_t* current;

  for (uintptr_t i = 0; i < DEFAULT_ZONE_ENTRY_SIZE_COUNT; i++) {

    current = &__initial_allocator[i];

    init_zone_allocator(current, 16 * Kib, __default_entry_sizes[i], ZALLOC_FLAG_KERNEL);

    ASSERT_MSG(current, "Failed to create kernel zone allocators");

    Must(__kallocators_add_allocator(current));
  }
}

#define DEFAULT_ZONE_STORE_CAPACITY ((SMALL_PAGE_SIZE - sizeof(zone_store_t)) >> 3)

ErrorOrPtr init_zone_allocator(zone_allocator_t* allocator, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags) {
  return init_zone_allocator_ex(allocator, nullptr, NULL, initial_size, hard_max_entry_size, flags);
}

ErrorOrPtr init_zone_allocator_ex(zone_allocator_t* ret, pml_entry_t* map, vaddr_t start_addr, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags)
{
  ErrorOrPtr result;
  size_t entries_for_this_zone;
  zone_t* new_zone;
  zone_store_t* new_zone_store;

  if (!ret || !initial_size || !hard_max_entry_size)
    return Error();

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

  new_zone = create_zone(hard_max_entry_size, entries_for_this_zone);

  if (!new_zone)
    goto fail_and_dealloc;

  result = zone_store_add(ret->m_store, new_zone);

  if (IsError(result))
    goto fail_and_dealloc;

  ret->m_total_size += new_zone->m_total_available_size;

  return Success(0);

fail_and_dealloc:
  kfree(ret);
  return Error();
}

zone_allocator_t* create_zone_allocator(size_t initial_size, size_t hard_max_entry_size, uintptr_t flags)
{
  return create_zone_allocator_ex(nullptr, 0, initial_size, hard_max_entry_size, flags);
}

zone_allocator_t* create_zone_allocator_ex(pml_entry_t* map, vaddr_t start_addr, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags) {

  zone_allocator_t *ret;

  if (!initial_size || !hard_max_entry_size)
    return nullptr;

  /* FIXME: as an allocator, we don't want to depend on another allocator to be created */
  ret = kmalloc(sizeof(zone_allocator_t));

  if (IsError(init_zone_allocator_ex(ret, map, start_addr, initial_size, hard_max_entry_size, flags))) {
    kfree(ret);
    return nullptr;
  }

  return ret;
}

void destroy_zone_allocator(zone_allocator_t* allocator, bool clear_zones) {

  // detach_allocator(allocator);

  destroy_zone_stores(allocator);

  //destroy_heap(allocator->m_heap);

  kfree(allocator);
}

/*!
 * @brief Try to add zones to the allocator
 *
 * We try really hard to add a new zone to this allocator, by either adding a new zone
 * to an exsisting store, or otherwise creating a new store to put the new zone into
 * @returns a pointer to the zone that was added to the allocator
 */
static ErrorOrPtr grow_zone_allocator(zone_allocator_t* allocator) {

  zone_t* new_zone;
  zone_store_t* new_zone_store;
  size_t grow_size;
  size_t entry_size;
  ErrorOrPtr result;

  if (!allocator)
    return Error();

  /*
   * Create a new zone to fit the new size
   */

  grow_size = allocator->m_grow_size;
  entry_size = allocator->m_entry_size;

  /* FIXME: integer devision */
  size_t entries_for_this_zone = (grow_size / entry_size);

  new_zone = create_zone(allocator->m_entry_size, entries_for_this_zone);

  /* Failed to create zone: probably out of physical memory */
  if (!new_zone)
    return Error();

  result = allocator_add_zone(allocator, new_zone);

  /*
   * If we where able to add the new zone, thats a successful growth.
   * Otherwise we need to add a new zone store...
   */
  if (!IsError(result))
    return Success((uintptr_t)new_zone);

  new_zone_store = create_zone_store(DEFAULT_ZONE_STORE_CAPACITY);

  if (!new_zone_store)
    return Error();

  result = zone_store_add(new_zone_store, new_zone);

  /* Could not even add a zone to the new store we just made.. this is bad =( */
  if (IsError(result))
    return Error();

  /* Yay we have a new store that also has a clean zone! lets add it to the allocator */

  new_zone_store->m_next = allocator->m_store;

  allocator->m_store = new_zone_store;
  allocator->m_store_count++;

  return Success((uintptr_t)new_zone);
}

zone_store_t* create_zone_store(size_t initial_capacity) {

  // We subtract the size of the entire zone_store header struct without 
  // the size of the m_zones field
  const size_t bytes = ALIGN_UP(initial_capacity * sizeof(zone_t*) + (sizeof(zone_store_t)), SMALL_PAGE_SIZE);
  /* The amount of bytes that we gain by aligning up */
  const size_t delta_bytes = (bytes - sizeof(zone_store_t)) - (initial_capacity * sizeof(zone_t*));
  const size_t delta_entries = delta_bytes ? (delta_bytes >> 3) : 0; // delta / sizeof(zone_t*)

  initial_capacity += delta_entries;

  zone_store_t* store;
  ErrorOrPtr result;

  result = __kmem_kernel_alloc_range(bytes, 0, KMEM_FLAG_WRITABLE);

  if (IsError(result))
    return nullptr;

  store = (zone_store_t*)Release(result);

  // set the counts
  store->m_zones_count = 0;
  store->m_next = nullptr;
  store->m_capacity = initial_capacity;

  // Make sure this zone store is uninitialised
  memset(store->m_zones, 0, initial_capacity * sizeof(zone_t*));

  return store;
}

void destroy_zone_store(zone_allocator_t* allocator, zone_store_t* store) {

  for (uintptr_t i = 0; i < store->m_zones_count; i++) {
    zone_t* zone = store->m_zones[i];

    /* Just to be sure yk */
    if (!zone)
      continue;

    destroy_zone(allocator, zone);
  }

  __kmem_kernel_dealloc((uintptr_t)store, store->m_capacity * sizeof(uintptr_t) + (sizeof(zone_store_t) - 8));
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

ErrorOrPtr allocator_add_zone(zone_allocator_t* allocator, zone_t* zone)
{
  ErrorOrPtr result;

  if (!allocator || !zone)
    return Error();

  FOREACH_ZONESTORE(allocator, store) {
    result = zone_store_add(store, zone);

    if (!IsError(result))
      return result;
  }

  return result;
}

ErrorOrPtr allocator_remove_zone(zone_allocator_t* allocator, zone_t* zone)
{
  ErrorOrPtr result;

  if (!allocator || !zone)
    return Error();

  FOREACH_ZONESTORE(allocator, store) {
    result = zone_store_remove(store, zone);

    if (!IsError(result))
      return result;
  }

  return result;
}

ErrorOrPtr zone_store_add(zone_store_t* store, zone_t* zone) {

  if (!store || !zone || !store->m_capacity)
    return Error();

  // More zones than we can handle?
  if (store->m_zones_count > store->m_capacity) {
    return Error();
  }

  store->m_zones[store->m_zones_count] = zone;

  store->m_zones_count++;

  return Success(0);
}

/* 
 * We assume the zone is already destroyed when we remove it
 */
ErrorOrPtr zone_store_remove(zone_store_t* store, zone_t* zone) {

  if (!store || !zone)
    return Error();

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
  if (index == (uintptr_t)-1) {
    return Error();
  }

  // Shift every entry after the one we want to remove back by one
  for (uintptr_t i = index; i < store->m_zones_count-1; i++) {
    store->m_zones[i] = store->m_zones[i+1];
  }

  store->m_zones_count--;

  return Success(0);
}

void destroy_zone(zone_allocator_t* allocator, zone_t* zone)
{
  /* TODO: use this to grab the pml root */
  (void)allocator;

  size_t zone_size = sizeof(zone_t);

  zone_size += zone->m_entries.m_size;
  zone_size += zone->m_total_available_size;

  /* TODO: resolve pml root */
  __kmem_dealloc(nullptr, nullptr, (vaddr_t)zone, zone_size);
}

/*
 * TODO: we can add some random padding between the zone_t struct
 * and where the actual memory storage starts
 */
zone_t* create_zone(const size_t entry_size, size_t max_entries) {
  ASSERT_MSG(entry_size != 0 && max_entries != 0, "create_zone: expected non-null parameters!");

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

    if (previous_extra_bytes == extra_bitmap_bytes) {
      break;
    }
  }

  max_entries += extra_entries;
  bitmap_bytes += extra_bitmap_bytes;

  zone_t* zone;
  ErrorOrPtr result = __kmem_kernel_alloc_range(aligned_size, 0, 0);

  if (IsError(result))
    return nullptr;

  zone = (zone_t*)Release(result);

  // We'll have to construct this bitmap ourselves
  zone->m_entries = (bitmap_t) {
    .m_entries = max_entries,
    .m_size = bitmap_bytes,
    .m_default = 0x00,
  };

  memset(zone->m_entries.m_map, 0, zone->m_entries.m_size);

  zone->m_zone_entry_size = entry_size;

  // Calculate where the memory actually starts
  const size_t entries_bytes = zone->m_entries.m_entries * zone->m_zone_entry_size;

  zone->m_entries_start = ((uintptr_t)zone + aligned_size) - entries_bytes;
  zone->m_total_available_size = entries_bytes;

  return zone;
}

void* kzalloc(size_t size)
{
  size_t allocator_size;
  zone_allocator_t* allocator;

  if (!size)
    return nullptr;

  allocator = __get_allocator_for_size(size);

  if (!allocator) {

    allocator_size = ZALLOC_DEFAULT_ALLOC_COUNT * size;

    /* Let's try to create a new allocator for this size */
    allocator = create_zone_allocator(allocator_size, size, ZALLOC_FLAG_KERNEL);

    Must(__kallocators_add_allocator(allocator));
  } 

  /*
  if (!allocator)
    return nullptr;
    */

  ASSERT_MSG(allocator, "Failed to find zone allocator for kernel allocation!");

  return zalloc(allocator, size);
}

void kzfree(void* address, size_t size)
{
  zone_allocator_t* allocator;

  if (!size)
    return;

  allocator = __get_allocator_for_size(size);

  /*
  if (!allocator)
    return;
    */

  ASSERT_MSG(allocator, "Failed to find zone allocator to free kernel allocation!");

  zfree(allocator, address, size);
}

void* zalloc(zone_allocator_t* allocator, size_t size) {

  ErrorOrPtr result;
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

        if (IsError(result))
          continue;

        return (void*)Release(result);
      }
    }

    current_store = current_store->m_next;
  }

  result = grow_zone_allocator(allocator);

  if (IsError(result))
    return nullptr;

  new_zone = (zone_t*)Release(result);

  if (!new_zone)
    return nullptr;

  result = zone_allocate(new_zone, size);

  if (!IsError(result))
    return (void*)Release(result);

  /* TODO: try to create a new zone somewhere and allocate there */
  kernel_panic("zalloc call failed! TODO: fix this dumb panic!");

  return nullptr;
}

void zfree(zone_allocator_t* allocator, void* address, size_t size) {

  ErrorOrPtr result;
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
        result = zone_deallocate(zone, address, size);

        if (IsError(result)) {
          continue;
        }

        // If the deallocation succeeded, just exit
        break;
      }
    }

    current_store = current_store->m_next;
  }
}

void* zalloc_fixed(zone_allocator_t* allocator) {
  return zalloc(allocator, allocator->m_entry_size);
}

void zfree_fixed(zone_allocator_t* allocator, void* address) {
  zfree(allocator, address, allocator->m_entry_size);
}

static ErrorOrPtr zone_allocate(zone_t* zone, size_t size) {
  ASSERT_MSG(size <= zone->m_zone_entry_size, "Allocating over the span of multiple subzones is not yet implemented!");

  // const size_t entries_needed = (size + zone->m_zone_entry_size - 1) / zone->m_zone_entry_size;
  ErrorOrPtr result = bitmap_find_free(&zone->m_entries);

  if (result.m_status == ANIVA_FAIL) {
    return Error();
  }

  uintptr_t index = result.m_ptr;
  bitmap_mark(&zone->m_entries, index);
  vaddr_t ret = zone->m_entries_start + (index * zone->m_zone_entry_size);
  return Success(ret);
}

static ErrorOrPtr zone_deallocate(zone_t* zone, void* address, size_t size) {
  ASSERT_MSG(size <= zone->m_zone_entry_size, "Deallocating over the span of multiple subzones is not yet implemented!");

  // Address is not contained inside this zone
  if ((uintptr_t)address < zone->m_entries_start) {
    return Error();
  }

  /*
   * FIXME: this integer division is dangerous. We really should only accept sizes that are
   * a power of two and if they are not, we should round them up to be that size. This way we can 
   * store the log2 of the entry_size which will make sure that we can ALWAYS find back the address
   * of any allocation
   */
  uintptr_t address_offset = (uintptr_t) (address - zone->m_entries_start) / zone->m_zone_entry_size;

  // If an offset is equal to or greater than the amount of entries, something has 
  // gone wrong, since offsets start at 0
  if (address_offset >= zone->m_entries.m_entries)
    return Error();

  if (!bitmap_isset(&zone->m_entries, address_offset))
    return Error();

  bitmap_unmark(&zone->m_entries, address_offset);

  memset(address, 0x00, size);

  return Success(0);
}
