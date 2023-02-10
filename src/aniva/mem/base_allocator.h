#ifndef __ANIVA_BASE_ALLOCATOR__
#define __ANIVA_BASE_ALLOCATOR__
#include <libk/stddef.h>
#include <libk/error.h>
#include "PagingComplex.h"

/*
 * the ironic part about this is that this
 * most likely needs to be allocated on a heap as well xD
 */

typedef void* (*HEAP_ALLOCATE) (
  size_t bytes
);

typedef void (*HEAP_DEALLOCATE) (
  void* address
);

typedef enum GHEAP_FLAGS {
  GHEAP_READONLY = (1 << 0),
  GHEAP_KERNEL = (1 << 1),
  GHEAP_EXPANDABLE = (1 << 2)
} GHEAP_FLAGS_t;

typedef struct generic_heap {
  vaddr_t m_virtual_base;
  paddr_t m_physical_base;
  size_t m_current_total_size;
  uintptr_t m_flags;

  HEAP_ALLOCATE f_allocate;
  HEAP_DEALLOCATE f_deallocate;
} generic_heap_t;

/*
 * map this heap for it to be ready to *in theory* accept allocations and
 * deallocations. in reality, it can never do this because the generic_heap
 * structure itself does not know how to allocate or deallocate anything
 */
generic_heap_t *initialize_generic_heap(PagingComplex_t* root_table, vaddr_t virtual_base, size_t initial_size, uintptr_t flags);

static ALWAYS_INLINE bool is_heap_identity_mapped(generic_heap_t* heap_ptr) {
  return (heap_ptr->m_physical_base == heap_ptr->m_virtual_base);
}

#endif //__ANIVA_BASE_ALLOCATOR__
