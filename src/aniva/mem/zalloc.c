#include "zalloc.h"
#include "heap.h"
#include "kmem_manager.h"
#include "dev/debug/serial.h"
#include "libk/bitmap.h"
#include "libk/error.h"
#include "libk/string.h"

void* zalloc(zone_allocator_t* allocator, size_t size);
void zfree(zone_allocator_t* allocator, void* address, size_t size);
void zexpand(zone_allocator_t* allocator, size_t extra_size);

static void* zone_allocate(zone_t* zone, size_t size);
static ErrorOrPtr zone_deallocate(zone_t* zone, void* address, size_t size);

static const enum ZONE_ENTRY_SIZE default_entry_sizes[DEFAULT_ZONE_ENTRY_SIZE_COUNT] = {
  [0] = ZALLOC_8BYTES,
  [1] = ZALLOC_16BYTES,
  [2] = ZALLOC_32BYTES,
  [3] = ZALLOC_64BYTES,
  [4] = ZALLOC_128BYTES,
  [5] = ZALLOC_256BYTES,
  [6] = ZALLOC_512BYTES,
};

zone_allocator_t *create_zone_allocator(size_t initial_size, uintptr_t flags) {
  zone_allocator_t *ret = kmalloc(sizeof(zone_allocator_t));

  // We don't need a virtual base yet. For now, we will just ask the 
  // kernel for some pages
  ret->m_heap = initialize_generic_heap(nullptr, 0, 0, flags);
  ret->m_heap->f_allocate = (HEAP_ALLOCATE) zalloc;
  ret->m_heap->f_sized_deallocate = (HEAP_SIZED_DEALLOCATE) zfree;
  ret->m_heap->f_expand = (HEAP_EXPAND) zexpand;
  ret->m_heap->m_parent_heap = ret;

  // Create a store that can hold the maximum amount of zones for 1 page
  ret->m_store = create_zone_store(512 - ZONE_STORE_DATA_FIELDS_SIZE);

  // Initialize the initial zones
  ret->m_heap->m_current_total_size = 0;

  for (uintptr_t i = 0; i < DEFAULT_ZONE_ENTRY_SIZE_COUNT; i++) {
    enum ZONE_ENTRY_SIZE entry_size = default_entry_sizes[i];

    size_t entries_for_this_zone = initial_size / entry_size;
    println(to_string(entries_for_this_zone));

    ErrorOrPtr result = zone_store_add(&ret->m_store, create_zone(entry_size, entries_for_this_zone / DEFAULT_ZONE_ENTRY_SIZE_COUNT));

    zone_t* zone = (zone_t*)Release(result);

    ret->m_heap->m_current_total_size += zone->m_total_available_size;
  }

  return ret;
}

void destroy_zone_allocator(zone_allocator_t* allocator, bool clear_zones) {
  kernel_panic("UNIMPLEMENTED: destroy_zone_allocator");
}

zone_store_t* create_zone_store(size_t initial_capacity) {

  // We subtract the size of the entire zone_store header struct without 
  // the size of the m_zones field
  const size_t bytes = initial_capacity * sizeof(uintptr_t) - (sizeof(zone_store_t) - 8);
  zone_store_t* store = (zone_store_t*)Must(kmem_kernel_alloc_range(bytes, 0, 0));

  // set the counts
  store->m_zones_count = 0;
  store->m_capacity = initial_capacity;

  // Make sure this zone store is uninitialised
  memset(store->m_zones, 0, initial_capacity * sizeof(uintptr_t));

  return store;
}

void destroy_zone_store(zone_store_t* store) {
  kmem_kernel_dealloc((uintptr_t)store, store->m_capacity * sizeof(uintptr_t) + (sizeof(zone_store_t) - 8));
}

ErrorOrPtr zone_store_add(zone_store_t** store_dptr, zone_t* zone) {
  zone_store_t* store = *store_dptr;
  // More zones than we can handle?
  if (store->m_zones_count > store->m_capacity) {
    // Reallocate
    kernel_panic("TODO: implement reallocating zone stores");
    
    return zone_store_add(store_dptr, zone);
  }

  store->m_zones[store->m_zones_count] = zone;

  store->m_zones_count++;

  return Success((uintptr_t)zone);
}

ErrorOrPtr zone_store_remove(zone_store_t* store, zone_t* zone) {

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

  return Success(0);
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

  print("Extra entries: ");
  println(to_string(extra_entries));

  max_entries += extra_entries;
  bitmap_bytes += extra_bitmap_bytes;

  zone_t* zone = (zone_t*)Release(kmem_kernel_alloc_range(aligned_size, 0, 0));

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

  for (uintptr_t i = 0; i < allocator->m_store->m_zones_count; i++) {
    zone_t* zone = allocator->m_store->m_zones[i];

    if (!zone)
      continue;

    if (size <= zone->m_zone_entry_size) {
      void* result = zone_allocate(zone, size);

      if (!result)
        continue;

      return result;
    }
  }
  return nullptr;
}

void zfree(zone_allocator_t* allocator, void* address, size_t size) {

  for (uintptr_t i = 0; i < allocator->m_store->m_zones_count; i++) {
    zone_t* zone = allocator->m_store->m_zones[i];

    if (!zone)
      continue;

    if (size <= zone->m_zone_entry_size) {
      ErrorOrPtr result = zone_deallocate(zone, address, size);

      if (result.m_status != ANIVA_SUCCESS) {
        continue;
      }

      // If the deallocation succeeded, just exit
      break;
    }
  }
}

void zexpand(zone_allocator_t* allocator, size_t extra_size) {
  kernel_panic("UNIMPLEMENTED: zexpand");
}


static void* zone_allocate(zone_t* zone, size_t size) {
  ASSERT_MSG(size <= zone->m_zone_entry_size, "Allocating over the span of multiple subzones is not yet implemented!");

  // const size_t entries_needed = (size + zone->m_zone_entry_size - 1) / zone->m_zone_entry_size;
  ErrorOrPtr result = bitmap_find_free(&zone->m_entries);

  if (result.m_status == ANIVA_FAIL) {
    return nullptr;
  }

  uintptr_t index = result.m_ptr;
  bitmap_mark(&zone->m_entries, index);
  vaddr_t ret = zone->m_entries_start + (index * zone->m_zone_entry_size);
  return (void*)ret;
}

static ErrorOrPtr zone_deallocate(zone_t* zone, void* address, size_t size) {
  ASSERT_MSG(size <= zone->m_zone_entry_size, "Deallocating over the span of multiple subzones is not yet implemented!");

  // Address is not contained inside this zone
  if ((uintptr_t)address < zone->m_entries_start) {
    return Error();
  }

  uintptr_t address_offset = (uintptr_t) (address - zone->m_entries_start) / zone->m_zone_entry_size;

  // If an offset is equal to the amount of entries, something has 
  // gone wrong, since offsets start at 0
  if (address_offset >= zone->m_entries.m_entries) {
    return Error();
  }

  memset(address, 0x00, size);

  bitmap_unmark(&zone->m_entries, address_offset);
  return Success(0);
}
