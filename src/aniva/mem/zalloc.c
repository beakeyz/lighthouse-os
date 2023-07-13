#include "zalloc.h"
#include "heap.h"
#include "kmem_manager.h"
#include "dev/debug/serial.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "libk/string.h"

/* Only the sized list needs to know what its end is */
zone_allocator_t* sized_end_allocator;
size_t sized_allocator_count;

zone_allocator_t* g_sized_zallocators;
zone_allocator_t* g_dynamic_zallocators;

void* zalloc(zone_allocator_t* allocator, size_t size);
void zfree(zone_allocator_t* allocator, void* address, size_t size);
void zexpand(zone_allocator_t* allocator, size_t extra_size);

static ErrorOrPtr zone_allocate(zone_t* zone, size_t size);
static ErrorOrPtr zone_deallocate(zone_t* zone, void* address, size_t size);

static const enum ZONE_ENTRY_SIZE default_entry_sizes[DEFAULT_ZONE_ENTRY_SIZE_COUNT] = {
  [0] = ZALLOC_8BYTES,
  [1] = ZALLOC_16BYTES,
  [2] = ZALLOC_32BYTES,
  [3] = ZALLOC_64BYTES,
  [4] = ZALLOC_128BYTES,
  [5] = ZALLOC_256BYTES,
  [6] = ZALLOC_512BYTES,
  [7] = ZALLOC_1024BYTES,
};

void init_zalloc() {
  sized_allocator_count = 0;
  sized_end_allocator = nullptr;
  g_sized_zallocators = nullptr;
  g_dynamic_zallocators = nullptr;
}

/*
 * Find the middle of a range using a fast and a slow pointer
 */
static zone_allocator_t** try_find_middle_zone_allocator(zone_allocator_t** start, zone_allocator_t** end) {

  zone_allocator_t** slow = start;
  zone_allocator_t** fast = start;

  while (*fast && (*fast)->m_next) {

    slow = &(*slow)->m_next;
    fast = &(*fast)->m_next->m_next;
  }

  return slow;
}

/*
 * Tries to find the specified allocator and returns the last 
 * slot if it isn't found
 */
static ErrorOrPtr try_attach_zone_allocator(zone_allocator_t* allocator, bool dyn) {

  zone_allocator_t* current;
  /* When we attach to the sized list, we need to make sure that we add it in order */
  zone_allocator_t** ret = &g_sized_zallocators;

  if (!allocator)
    return Error();

  if (dyn)
    ret = &g_dynamic_zallocators;

  for (; *ret; ret = &(*ret)->m_next) {
    current = *ret;

    /* Found the last sized allocator =D */
    if (!current->m_next) {
      sized_end_allocator = allocator;
      continue;
    }

    /*
     * If we reached the end, we just return that. Otherwise 
     * we check if we have reached the end of our size
     */
    if ((current->m_next && (current->m_max_entry_size == allocator->m_max_entry_size &&
        current->m_next->m_max_entry_size > allocator->m_max_entry_size)) && 
        !dyn) {

      allocator->m_next = current->m_next;

      break;
    }
    /* The sized allocator check get priority here */
    else if (current == allocator)
      break;

  }

  if (*ret)
    return Error();

  *ret = allocator;

  return Success(0);
}

static zone_allocator_t** try_find_last_of_size(size_t zone_size) {

  bool greater_size;
  zone_allocator_t* current_start = g_sized_zallocators;
  zone_allocator_t* current_end = sized_end_allocator;

  while (current_start && current_end) {

    zone_allocator_t** middle = try_find_middle_zone_allocator(&current_start, &current_end);

    if (!middle || !(*middle))
      return nullptr;

    /* Found it! */
    if ((*middle)->m_max_entry_size == zone_size) {

      /* Let's loop until the next size is different */

      zone_allocator_t** itt = middle;

      for (; *itt; itt = &(*itt)->m_next) {
        const zone_allocator_t* allocator = *itt;

        if (!allocator->m_next && allocator->m_max_entry_size == zone_size) {
          break;
        }

        if (allocator->m_next->m_max_entry_size != zone_size) {

          ASSERT_MSG(allocator->m_max_entry_size < (*itt)->m_next->m_max_entry_size, "try_find_last_of_size: found middle of size, but there the next size was not in order!");
          
          break;
        }
      }

      return itt;
    }

    greater_size = (*middle)->m_max_entry_size > zone_size;

    /* Our target size is to the 'left' of this allocator */
    if (greater_size) {
      current_end = *middle;
    } else {
      current_start = *middle;
    }
  }

  return nullptr;
}

static bool is_dynamic(zone_allocator_t* allocator) {

  ASSERT_MSG(allocator, "is_dynamic: passed nullptr as allocator!");

  return (allocator->m_flags & ZALLOC_FLAG_DYNAMIC) == ZALLOC_FLAG_DYNAMIC;
}

static ErrorOrPtr attach_allocator(zone_allocator_t* allocator) {
  /* Check if we are dynamic and emplace in the correct list */

  size_t allocator_size;
  bool dyn = is_dynamic(allocator);

  if (!dyn) {
    /* Attach after the last allocator of the same size */
    ASSERT_MSG(allocator->m_max_entry_size == allocator->m_min_entry_size, "attach_allocator: allocator size mismatch! (min != max)");
    ASSERT_MSG(allocator->m_max_entry_size, "attach_allocator: allocator has no size!");

    allocator_size = allocator->m_max_entry_size;

    zone_allocator_t** last = try_find_last_of_size(allocator_size);

    /* We prob don't have this size yet, add it somewhere in order */
    if (!last) {
      goto raw_attach;
    }

    zone_allocator_t* next = (*last)->m_next;

    /* Attach */
    (*last)->m_next = allocator;
    allocator->m_next = next;

    /* this is the new last allocator */
    if (next == nullptr) {
      sized_end_allocator = allocator;
    }

    return Success(0);
  }

raw_attach:;
  /* Find the last */
  return try_attach_zone_allocator(allocator, dyn);
}

static void detach_allocator(zone_allocator_t* allocator) {

  zone_allocator_t** allocator_list;

  if (!allocator)
    return;

  allocator_list = &g_sized_zallocators;

  /* Check if we are dynamic and detach from the correct list */
  if (is_dynamic(allocator)) {
    allocator_list = &g_dynamic_zallocators;
  }

  /* TODO: use binary search instead of linear scanning */
  for (; *allocator_list; allocator_list = &(*allocator_list)->m_next) {
    /* Found our entry */
    if (*allocator_list == allocator) {
      *allocator_list = allocator->m_next;
      break;
    }
  }
}

zone_allocator_t *create_dynamic_zone_allocator(size_t initial_size, uintptr_t flags) 
{
  return create_zone_allocator_ex(nullptr, 0, initial_size, 0, ZALLOC_FLAG_DYNAMIC | flags);
}

zone_allocator_t* create_zone_allocator(size_t initial_size, size_t hard_max_entry_size, uintptr_t flags)
{
  return create_zone_allocator_ex(nullptr, 0, initial_size, hard_max_entry_size, flags);
}

#define DEFAULT_ZONE_STORE_CAPACITY (512 - sizeof(zone_store_t))

zone_allocator_t* create_zone_allocator_ex(pml_entry_t* map, vaddr_t start_addr, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags) {

  /* FIXME: as an allocator, we don't want to depend on another allocator to be created */
  zone_allocator_t *ret = kmalloc(sizeof(zone_allocator_t));

  // We don't need a virtual base yet. For now, we will just ask the 
  // kernel for some pages
  /* TODO: map should be used here */
  /* TODO: start_addr should be used here */

  // Initialize the initial zones
  ret->m_total_size = 0;
  ret->m_grow_size = initial_size;

  // Create a store that can hold the maximum amount of zones for 1 page
  ret->m_store = create_zone_store(DEFAULT_ZONE_STORE_CAPACITY);
  ret->m_flags = flags;

  /* Calculate how big this zone needs to be */

  ret->m_min_entry_size = ZALLOC_MIN_ZONE_ENTRY_SIZE;
  ret->m_max_entry_size = ZALLOC_MAX_ZONE_ENTRY_SIZE;

  if (is_dynamic(ret)) {

    for (uintptr_t i = 0; i < DEFAULT_ZONE_ENTRY_SIZE_COUNT; i++) {
      enum ZONE_ENTRY_SIZE entry_size = default_entry_sizes[i];

      size_t entries_for_this_zone = (initial_size / entry_size) / DEFAULT_ZONE_ENTRY_SIZE_COUNT + 1;
      println(to_string(entries_for_this_zone));

      ErrorOrPtr result = zone_store_add(ret->m_store, create_zone(entry_size, entries_for_this_zone));

      zone_t* zone = (zone_t*)Release(result);

      ret->m_total_size += zone->m_total_available_size;
    }

    goto end_and_attach;
  } 

  ret->m_max_entry_size = hard_max_entry_size;
  ret->m_min_entry_size = hard_max_entry_size;

  /* FIXME: integer devision */
  size_t entries_for_this_zone = (initial_size / hard_max_entry_size);

  ErrorOrPtr result = zone_store_add(ret->m_store, create_zone(hard_max_entry_size, entries_for_this_zone));

  zone_t* zone = (zone_t*)Release(result);

  ret->m_total_size += zone->m_total_available_size;

end_and_attach:
  /* NOTE: attach after we've setup the zones */
  //println("Trying to attach");
  //Must(attach_allocator(ret));
  //println("did attach");

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
  entry_size = allocator->m_max_entry_size;

  /* FIXME: integer devision */
  size_t entries_for_this_zone = (grow_size / entry_size);

  new_zone = create_zone(allocator->m_max_entry_size, entries_for_this_zone);

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
  const size_t bytes = initial_capacity * sizeof(uintptr_t) - (sizeof(zone_store_t) - 8);
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
  memset(store->m_zones, 0, initial_capacity * sizeof(uintptr_t));

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

  if (!store || !store->m_capacity)
    return Error();

  // More zones than we can handle?
  if (store->m_zones_count > store->m_capacity) {
    return Error();
  }

  store->m_zones[store->m_zones_count] = zone;

  store->m_zones_count++;

  return Success((uintptr_t)zone);
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
  if ((allocator->m_flags & ZALLOC_FLAG_DYNAMIC) == ZALLOC_FLAG_DYNAMIC)
    return nullptr;

  return zalloc(allocator, allocator->m_max_entry_size);
}

void zfree_fixed(zone_allocator_t* allocator, void* address) {
  if ((allocator->m_flags & ZALLOC_FLAG_DYNAMIC) == ZALLOC_FLAG_DYNAMIC)
    return;

  zfree(allocator, address, allocator->m_max_entry_size);
}

void zexpand(zone_allocator_t* allocator, size_t extra_size) {
  kernel_panic("UNIMPLEMENTED: zexpand");
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
