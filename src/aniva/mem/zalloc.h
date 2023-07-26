#ifndef __ANIVA_ZALLOC__
#define __ANIVA_ZALLOC__
#include <libk/stddef.h>
#include <libk/flow/error.h>
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

/* These are the default entry sizes */
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

#define DEFAULT_ZONE_ENTRY_SIZE_COUNT   8

/* Create a zone allocator that fits dynamicly sized object */
#define ZALLOC_FLAG_KERNEL              (0x00000001)

#define ZALLOC_DEFAULT_MEM_SIZE         16 * Kib
#define ZALLOC_DEFAULT_ALLOC_COUNT      128 /* What is the default amount of an object that we should be able to allocate before we should expand the allocator */
#define ZALLOC_ACCEPTABLE_MEMSIZE_DEVIATON 4 /* How much a allocation may deviate from the size of a allocator in order to still be stored there */


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
  struct zone_store* m_next; /* When a store is full, we can allocate a new store and link it after this one */
  zone_t* m_zones[];
} __attribute__((packed)) zone_store_t;

/*
 * TODO (?): We could expand this object so that it becomes viable to store it completely
 * independently from any other kind of heap. The only dependency it would have then is the 
 * physical allocator
 */
typedef struct zone_allocator {
  //generic_heap_t* m_heap;

  zone_store_t* m_store;
  size_t m_store_count;

  size_t m_grow_size; /* Size we add to the allocator every time we grow */
  size_t m_total_size;
  uint32_t m_flags;
  uint32_t m_res0;

  enum ZONE_ENTRY_SIZE m_entry_size;

  /* We link zone allocators in a global list that we sort based on 
   * Their zone size. If they are dynamic, this links into a seperate
   * list for those
   */
  struct zone_allocator* m_next;
} zone_allocator_t;

#define FOREACH_ZONESTORE(allocator, i) for (zone_store_t* i = allocator->m_store; i != nullptr; i = i->m_next)

void init_zalloc();

void* kzalloc(size_t size);
void kzfree(void* address, size_t size);

void* zalloc(zone_allocator_t* allocator, size_t size);
void zfree(zone_allocator_t* allocator, void* address, size_t size);

void* zalloc_fixed(zone_allocator_t* allocator);
void zfree_fixed(zone_allocator_t* allocator, void* address);

/*
 * make a new zone allocator. based on the initial_size, we will
 * saturate this allocator with zones as needed
 */
zone_allocator_t* create_zone_allocator(size_t initial_size, size_t hard_max_entry_size, uintptr_t flags);
zone_allocator_t* create_zone_allocator_at(vaddr_t start_addr, size_t initial_size, uintptr_t flags);
zone_allocator_t* create_zone_allocator_ex(pml_entry_t* map, vaddr_t start_addr, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags);

ErrorOrPtr init_zone_allocator(zone_allocator_t* allocator, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags);
ErrorOrPtr init_zone_allocator_ex(zone_allocator_t* allocator, pml_entry_t* map, vaddr_t start_addr, size_t initial_size, size_t hard_max_entry_size, uintptr_t flags);

void destroy_zone_allocator(zone_allocator_t* allocator, bool clear_zones);

zone_store_t* create_zone_store(size_t initial_capacity);

void destroy_zone_store(zone_allocator_t* allocator, zone_store_t* store);
void destroy_zone_stores(zone_allocator_t* allocator);

ErrorOrPtr allocator_add_zone(zone_allocator_t* allocator, zone_t* zone);
ErrorOrPtr allocator_remove_zone(zone_allocator_t* allocator, zone_t* zone);

ErrorOrPtr zone_store_add(zone_store_t* store, zone_t* zone);
ErrorOrPtr zone_store_remove(zone_store_t* store, zone_t* zone);

/*
 * Allocate a number of pages for this zone.
 * max_entries gets rounded up in order to fit 
 * the amount of pages allocated
 */
zone_t* create_zone(const size_t entry_size, size_t max_entries);

void destroy_zone(zone_allocator_t* allocator, zone_t* zone);

#endif //__ANIVA_ZALLOC__
