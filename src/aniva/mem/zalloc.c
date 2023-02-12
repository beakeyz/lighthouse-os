#include "zalloc.h"
#include "heap.h"
#include "kmem_manager.h"
#include "dev/debug/serial.h"
#include "libk/string.h"

static void* zone_allocate(zone_t* zone, size_t size);
static void zone_deallocate(zone_t* zone, void* address, size_t size);
static ZALLOC_SIZE_t get_used_zone_for_size(size_t size);

const ZALLOC_SIZE_t zone_sizes[] = {
  ZALLOC_8BYTES,
  ZALLOC_16BYTES,
  ZALLOC_32BYTES,
  ZALLOC_64BYTES,
  ZALLOC_128BYTES,
  ZALLOC_256BYTES,
  ZALLOC_512BYTES,
};

zone_allocator_t *create_zone_allocator(size_t initial_size, vaddr_t virtual_base, uintptr_t flags) {
  zone_allocator_t *ret = kmalloc(sizeof(zone_allocator_t));
  ret->m_heap = initialize_generic_heap(nullptr, virtual_base, initial_size, flags);
  ret->m_heap->f_allocate = (HEAP_ALLOCATE) zalloc;
  ret->m_heap->f_sized_deallocate = (HEAP_SIZED_DEALLOCATE) zfree;
  ret->m_heap->f_expand = (HEAP_EXPAND) zexpand;

  if (initial_size < SMALL_PAGE_SIZE) {
    // TODO: we need to figure out how to index less than a page of zone-heap
  } else {
    const size_t pages_count = (ALIGN_DOWN(initial_size, SMALL_PAGE_SIZE)) / SMALL_PAGE_SIZE;
    const size_t zone_sizes_entries[] = {
      ZALLOC_MINIMAL_8BYTE_ENTRIES * pages_count,
      ZALLOC_MINIMAL_16BYTE_ENTRIES * pages_count,
      ZALLOC_MINIMAL_32BYTE_ENTRIES * pages_count,
      ZALLOC_MINIMAL_64BYTE_ENTRIES * pages_count,
      ZALLOC_MINIMAL_128BYTE_ENTRIES * pages_count,
      ZALLOC_MINIMAL_256BYTE_ENTRIES * pages_count,
      ZALLOC_MINIMAL_512BYTE_ENTRIES * pages_count,
    };

    size_t total_byte_offset = 0;

    for (uintptr_t i = 0; i < ZALLOC_POSSIBLE_SIZES_COUNT; i++) {
      const size_t zone_size_bytes = (zone_sizes_entries[i] + 8 - 1) / 8;

      ret->m_zones[i].m_zone_entry_size = zone_sizes[i];
      ret->m_zones[i].m_entry_count = zone_sizes_entries[i];
      ret->m_zones[i].m_total_zone_size = zone_size_bytes;
      ret->m_zones[i].m_entries = init_bitmap_with_default(zone_size_bytes, 0x00);
      ret->m_zones[i].m_virtual_base_address = ALIGN_UP(virtual_base + total_byte_offset, zone_sizes[i]);
      total_byte_offset += zone_size_bytes;
    }
  }

  return ret;
}

void* zalloc(zone_allocator_t* allocator, size_t size) {
  for (int i = 0; i < ZALLOC_POSSIBLE_SIZES_COUNT; i++) {
    zone_t* zone = &allocator->m_zones[i];

    if (size <= zone->m_zone_entry_size) {
      return zone_allocate(zone, size);
    }
  }
  return nullptr;
}

void zfree(zone_allocator_t* allocator, void* address, size_t size) {
  for (int i = 0; i < ZALLOC_POSSIBLE_SIZES_COUNT; i++) {
    zone_t* zone = &allocator->m_zones[i];

    if (size <= zone->m_zone_entry_size) {
      zone_deallocate(zone, address, size);
      return;
    }
  }
}

void zexpand(zone_allocator_t* allocator, size_t extra_size) {
  // TODO: impl
}


static void* zone_allocate(zone_t* zone, size_t size) {
  // TODO: perhaps support allocations over multiple entries?
  const size_t entries_needed = (size + zone->m_zone_entry_size - 1) / zone->m_zone_entry_size;
  ErrorOrPtr result = zone->m_entries.fFindFree(&zone->m_entries);

  if (result.m_status == ANIVA_FAIL) {
    return nullptr;
  }

  uintptr_t index = result.m_ptr;
  zone->m_entries.fMark(&zone->m_entries, index);
  vaddr_t ret = zone->m_virtual_base_address + (index * zone->m_zone_entry_size);
  return (void*)ret;
}

static void zone_deallocate(zone_t* zone, void* address, size_t size) {
  uintptr_t address_offset = (uintptr_t) (address - zone->m_virtual_base_address) / zone->m_zone_entry_size;

  memset(address, 0x00, size);

  if (zone->m_entries.fIsSet(&zone->m_entries, address_offset)) {
    zone->m_entries.fUnmark(&zone->m_entries, address_offset);
  }
}

static ZALLOC_SIZE_t get_used_zone_for_size(size_t size) {
  // TODO:
}