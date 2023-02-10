#ifndef __ANIVA_ZALLOC__
#define __ANIVA_ZALLOC__
#include <libk/stddef.h>
#include <libk/error.h>
#include "base_allocator.h"

typedef struct zone_allocator {

  generic_heap_t* m_heap;
} zone_allocator_t;

#endif //__ANIVA_ZALLOC__
