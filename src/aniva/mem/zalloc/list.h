#ifndef __ANIVA_MEM_ZALLOC_LIST__
#define __ANIVA_MEM_ZALLOC_LIST__

#include "libk/flow/error.h"
#include <libk/stddef.h>

struct zone_allocator;

typedef struct zalloc_list {
  struct zone_allocator* list;
  /* How many allocators does this list hold */
  uint32_t allocator_count;
  /* How many pages does this struct take up */
  uint32_t this_pagecount;
  /* Index into the cache for the last allocation */
  uintptr_t last_allocation_idx;

  /* A cache for recent allocations */
  struct {
    void* allocation;
    struct zone_allocator* allocator;
  } cache[];
} zalloc_list_t;

void* zalloc_listed(zalloc_list_t* list, size_t size);
void zfree_listed(zalloc_list_t* list, void* address, size_t size);
kerror_t zfree_listed_scan(zalloc_list_t* list, void* address);

zalloc_list_t* create_zalloc_list(uint32_t pagecount);
void destroy_zalloc_list(zalloc_list_t* list);


#endif // !__ANIVA_MEM_ZALLOC_LIST__
