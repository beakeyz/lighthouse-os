#ifndef __ANIVA_ZALLOC__
#define __ANIVA_ZALLOC__
#include <libk/stddef.h>
#include <libk/flow/error.h>
#include "base_allocator.h"
#include "libk/data/bitmap.h"
#include "mem/pg.h"

/*
 * Flags for an indevidual zone
 */
typedef enum ZONE_FLAGS {
  ZONE_USED = (1 << 0),
  ZONE_PERMANENT = (1 << 1),
  ZONE_SHOULD_ZERO = (1 << 2)
} ZONE_FLAGS_t;

typedef enum ZONE_ENTRY_SIZE {
  ZALLOC_8BYTES = 8,
  ZALLOC_16BYTES = 16,
  ZALLOC_32BYTES = 32,
  ZALLOC_64BYTES = 64,
  ZALLOC_128BYTES = 128,
  ZALLOC_256BYTES = 256,
  ZALLOC_512BYTES = 512,
  ZALLOC_1024BYTES = 1024,
  // ZALLOC_4096BYTES = 0x1000,
} ZONE_ENTRY_SIZE_t;

#define ZALLOC_MIN_ZONE_ENTRY_SIZE (ZALLOC_8BYTES)
#define ZALLOC_MAX_ZONE_ENTRY_SIZE (ZALLOC_1024BYTES)

/* Create a zone allocator that fits dynamicly sized object */
#define ZALLOC_FLAG_DYNAMIC             (0x00000001)
#define ZALLOC_FLAG_FIXED_SIZE          (0x00000002)

#define DEFAULT_ZONE_ENTRY_SIZE_COUNT   8

#define DEFAULT_ZALLOC_MEM_SIZE         2 * Mib


/*
 * Ideas for a different zalloc design:
 * a zone allocator can consist of multiple zones.
 * each zone acts as its own sub-allocator and is given
 * a contigous memory region to work with. At the top of this 
 * region is a header, which contains information about this zone, like size, bitmap, blocksize, ect.
 * 
 * When an allocation can not be satisfied in one of the zone, we ask kmem_manager for a number
 * of pages which we can populate with a new zone. Depending on the size of the allocation, we 
 * will choose a blocksize for this zone that matches this allocation. large allocation get larger
 * blocksizes, and smaller ones get smaller blocksizes.
 *
 * so in order of management:
 * main zone_allocator -> zone -> block -> physical memory
 *
 *  - the main allocator has a list of where all the zones are
 *  - the zone knows how big it is and where the blocks start
 *  - a block knows nothing about itself and is simply a pointer into a physical address. 
 *    it is managed by its coresponding chunk
 *
 * Current NOTEs
 *  - the current implementation is very agressive when it comes to page allocation. If, due to alignement, it 
 *    is able to snatch a few more pages, it won't hesitate to fill that empty space with more subzones/blocks/entries.
 */
typedef struct zone {
  size_t m_zone_entry_size;
  size_t m_total_available_size;

  vaddr_t m_entries_start;

  bitmap_t m_entries;
} zone_t;

/*
 * This header resides at the bottom of a page (or pages) that
 * store where all of our zones are located. One 4Kib page can store
 * up to 512 - x zones, where x is the amount of uintptrs come before the 
 * actual list (i.e. the field m_zones_count takes up 8 bytes and thus with
 * only that datafield we can store up to 511 zone locations in a singel page.)
 * 
 * When we want more than that amount of pages, we will have to find a new set of
 * pages where we can store more pointers to zones. Only the bottom page will
 * contain the header, so with every extra page, we can store 512 zones extra
 */
typedef struct zone_store {
  size_t m_zones_count;
  size_t m_capacity;
  zone_t* m_zones[];
} __attribute__((packed)) zone_store_t;

#define ZONE_STORE_DATA_FIELDS_SIZE (sizeof(zone_store_t) - sizeof(uintptr_t))

typedef struct zone_allocator {
  generic_heap_t* m_heap;

  zone_store_t* m_store;

  uint32_t m_flags;

  enum ZONE_ENTRY_SIZE m_max_zone_size;
  enum ZONE_ENTRY_SIZE m_min_zone_size;

  /* We link zone allocators in a global list that we sort based on 
   * Their zone size. If they are dynamic, this links into a seperate
   * list for those
   */
  struct zone_allocator* m_next;
} zone_allocator_t;

/* Sorted by size, as specified above. */
extern zone_allocator_t* g_sized_zallocators;
/* These are mostly allocators for threads and such, so this can grow quite large as there are more threads f.e */
extern zone_allocator_t* g_dynamic_zallocators;

void init_zalloc();

void* zalloc(zone_allocator_t* allocator, size_t size);
void zfree(zone_allocator_t* allocator, void* address, size_t size);

void* zalloc_fixed(zone_allocator_t* allocator);
void zfree_fixed(zone_allocator_t* allocator, void* address);

/*
 * make a new zone allocator. based on the initial_size, we will
 * saturate this allocator with zones as needed
 */
zone_allocator_t *create_dynamic_zone_allocator(size_t initial_size, uintptr_t flags);
zone_allocator_t* create_zone_allocator_at(vaddr_t start_addr, size_t initial_size, uintptr_t flags);
zone_allocator_t* create_zone_allocator_ex(pml_entry_t* map, vaddr_t start_addr, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags);

void destroy_zone_allocator(zone_allocator_t* allocator, bool clear_zones);

zone_store_t* create_zone_store(size_t initial_capacity);

void destroy_zone_store(zone_store_t* store);

ErrorOrPtr zone_store_add(zone_store_t** store_dptr, zone_t* zone);
ErrorOrPtr zone_store_remove(zone_store_t* store, zone_t* zone);

/*
 * Allocate a number of pages for this zone.
 * max_entries gets rounded up in order to fit 
 * the amount of pages allocated
 */
zone_t* create_zone(const size_t entry_size, size_t max_entries);

void destroy_zone(zone_t* zone);

#endif //__ANIVA_ZALLOC__
