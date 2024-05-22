#ifndef __LIGHTOS_LIBC_HEAP__
#define __LIGHTOS_LIBC_HEAP__

#include <sys/types.h>

/*
 * Main header for the heap used in regular lightos applications
 *
 * The first initialized heap will be considered the main heap, but complex applications may create 
 * seperate heaps for seperate subsystems.
 */

typedef struct lightos_heap lightos_heap_t;

/*!
 * @brief: Initialize a single lightos heap object
 *
 * @object_size: If this is NULL, we create a qd heap, which can host a multiple of allocation sizes
 * at the same time. If @object_size is set, this heap will be locked to objects with this size
 */
int init_lightos_heap(lightos_heap_t** pheap, size_t initial_size, size_t object_size);
int destroy_lightos_heap(lightos_heap_t* heap);

int lightos_heap_allocate(lightos_heap_t* heap, void** pbuf, size_t size);
int lightos_heap_allocate_n(lightos_heap_t* heap, void** pbuf, size_t size, uint32_t count);
int lightos_heap_deallocate(lightos_heap_t* heap, void* buf, size_t size);

int lightos_heap_get_allocation_size(lightos_heap_t* heap, void* ptr, size_t* psize);

#endif // !__LIGHTOS_LIBC_HEAP__
