#include "zalloc.h"
#include "heap.h"

static void* zone_allocate(zone_t* zone, size_t size);

zone_allocator_t *create_zone_allocator(size_t initial_size, vaddr_t virtual_base, uintptr_t flags) {
  zone_allocator_t *ret = kmalloc(sizeof(zone_allocator_t));
  ret->m_heap = initialize_generic_heap(nullptr, virtual_base, initial_size, flags);
  ret->m_heap->f_allocate = (HEAP_ALLOCATE) zalloc;
  ret->m_heap->f_deallocate = nullptr;
  ret->m_heap->f_sized_deallocate = (HEAP_SIZED_DEALLOCATE) zfree;
  ret->m_heap->f_expand = (HEAP_EXPAND) zexpand;

  // TODO
}

void* zalloc(zone_allocator_t* allocator, size_t size) {
  for (int i = 0; i < ZALLOC_POSSIBLE_SIZES_COUNT; i++) {
    zone_t zone = allocator->m_zones[i];

    if (zone.m_zone_entry_size > size) {
      return zone_allocate(&zone, size);
    }
  }
  return nullptr;
}

void zfree(zone_allocator_t* allocator, void* address, size_t size) {

}

void zexpand(zone_allocator_t* allocator, size_t extra_size) {

}

static void* zone_allocate(zone_t* zone, size_t size) {
  // TODO: perhaps support allocations over multiple entries?
  const size_t entries_needed = (size + zone->m_zone_entry_size - 1) / zone->m_zone_entry_size;
  ErrorOrPtr result = zone->m_entries->fFindFree(zone->m_entries);

  if (result.m_status == ANIVA_FAIL) {
    return nullptr;
  }

  uintptr_t index = result.m_ptr;
  zone->m_entries->fMark(zone->m_entries, index);
  vaddr_t ret = zone->m_virtual_base_address + (index * zone->m_zone_entry_size);
  return (void*)ret;
}