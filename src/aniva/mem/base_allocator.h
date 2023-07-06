#ifndef __ANIVA_BASE_ALLOCATOR__
#define __ANIVA_BASE_ALLOCATOR__
#include <libk/stddef.h>
#include <libk/flow/error.h>
#include "dev/debug/serial.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "pg.h"

struct mutex;

/*
 * the ironic part about this is that this
 * most likely needs to be allocated on a heap as well xD
 */

// heap_ptr should be of the implemented heap type
typedef void* (*HEAP_ALLOCATE) (
  void* heap_ptr,
  size_t bytes
);

// heap_ptr should be of the implemented heap type
typedef void (*HEAP_DEALLOCATE) (
  void* heap_ptr,
  void* address
);

typedef void (*HEAP_SIZED_DEALLOCATE) (
  void* heap_ptr,
  void* address,
  size_t allocation_size
);

typedef ANIVA_STATUS (*HEAP_EXPAND) (
  void* heap_ptr,
  size_t bytes
);

typedef void (*HEAP_ON_EXPAND_ENABLE) (
  void* heap_ptr
);

typedef void (*HEAP_GENERAL_DEBUG) (
  void* heap_ptr
);

typedef enum GHEAP_FLAGS {
  GHEAP_READONLY = (1 << 0),
  GHEAP_KERNEL = (1 << 1),
  GHEAP_EXPANDABLE = (1 << 2),
  GHEAP_ZEROED = (1 << 3),
  GHEAP_NOBLOCK = (1 << 4),
} GHEAP_FLAGS_t;

typedef struct generic_heap {

  void* m_parent_heap;
  vaddr_t m_virtual_base;
  paddr_t m_physical_base;
  size_t m_current_total_size;
  size_t m_hard_max_size;
  uintptr_t m_flags;

  struct mutex* m_lock;

  HEAP_ALLOCATE f_allocate;
  HEAP_DEALLOCATE f_deallocate;
  HEAP_SIZED_DEALLOCATE f_sized_deallocate;
  HEAP_EXPAND f_expand;
  HEAP_GENERAL_DEBUG f_debug;
  HEAP_ON_EXPAND_ENABLE f_on_expand_enable;

} generic_heap_t;

/*
 * map this heap for it to be ready to *in theory* accept allocations and
 * deallocations. in reality, it can never do this because the generic_heap
 * structure itself does not know how to allocate or deallocate anything
 */
generic_heap_t *initialize_generic_heap(pml_entry_t* root_table, vaddr_t virtual_base, size_t initial_size, uintptr_t flags);

ErrorOrPtr destroy_heap(generic_heap_t* heap);

static void enable_heap_expantion(generic_heap_t* heap) {
  if ((heap->m_flags & GHEAP_EXPANDABLE))
    return;

  heap->m_flags |= GHEAP_EXPANDABLE;

  if (heap->f_on_expand_enable) {
    heap->f_on_expand_enable(heap->m_parent_heap);
  }
}

static ALWAYS_INLINE bool is_heap_identity_mapped(generic_heap_t* heap_ptr) {
  return (heap_ptr->m_physical_base == heap_ptr->m_virtual_base);
}

#endif //__ANIVA_BASE_ALLOCATOR__
