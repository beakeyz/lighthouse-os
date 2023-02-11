#ifndef __ANIVA_ZALLOC__
#define __ANIVA_ZALLOC__
#include <libk/stddef.h>
#include <libk/error.h>
#include "base_allocator.h"
#include "libk/bitmap.h"

typedef enum ZONE_FLAGS {
  ZONE_USED = (1 << 0),
  ZONE_PERMANENT = (1 << 1),
  ZONE_SHOULD_ZERO = (1 << 2)
} ZONE_FLAGS_t;

/*
 * these values are used when constructing the zone allocator
 * structure. we should use slab for small allocations, so we can
 * divide this in such a way that we can at minimum cover
 * SMALL_PAGE_SIZE of bytes when allocating with this
 */
#define ZALLOC_MINIMAL_8BYTE_ENTRIES 512
#define ZALLOC_MINIMAL_16BYTE_ENTRIES 256
#define ZALLOC_MINIMAL_32BYTE_ENTRIES 128
#define ZALLOC_MINIMAL_64BYTE_ENTRIES 64
#define ZALLOC_MINIMAL_128BYTE_ENTRIES 32
#define ZALLOC_MINIMAL_256BYTE_ENTRIES 16
#define ZALLOC_MINIMAL_512BYTE_ENTRIES 8

#define ZALLOC_POSSIBLE_SIZES_COUNT 7

typedef enum ZALLOC_SIZE {
  ZALLOC_8BYTES = 8,
  ZALLOC_16BYTES = 16,
  ZALLOC_32BYTES = 32,
  ZALLOC_64BYTES = 64,
  ZALLOC_128BYTES = 128,
  ZALLOC_256BYTES = 256,
  ZALLOC_512BYTES = 512
} ZALLOC_SIZE_t;

typedef struct zone {
  ZALLOC_SIZE_t m_zone_entry_size;

  bitmap_t* m_entries;

  vaddr_t m_virtual_base_address;

  size_t m_entry_count;
  size_t m_total_zone_size;
} zone_t;

typedef struct zone_allocator {
  generic_heap_t* m_heap;

  zone_t m_zones[ZALLOC_POSSIBLE_SIZES_COUNT];

  ZALLOC_SIZE_t m_max_zone_size;
  ZALLOC_SIZE_t m_min_zone_size;
} zone_allocator_t;

/*
 * make a new zone allocator. based on the initial_size, we will
 * saturate this allocator with zones as needed
 */
zone_allocator_t *create_zone_allocator(size_t initial_size, vaddr_t virtual_base, uintptr_t flags);

void* zalloc(zone_allocator_t* allocator, size_t size);
void zfree(zone_allocator_t* allocator, void* address, size_t size);
void zexpand(zone_allocator_t* allocator, size_t extra_size);

#endif //__ANIVA_ZALLOC__
