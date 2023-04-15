#ifndef __ANIVA_HEAP_CORE__
#define __ANIVA_HEAP_CORE__
#include <libk/stddef.h>
#include <libk/error.h>
#include "base_allocator.h"

/*
 * initialize the first kernel heap manually. after this
 * heaps can be dynamically and automatically created
 */
void init_kheap();
void kheap_enable_expand();

/*
 * Generic heap allocation
 */
void* kmalloc (size_t len);

/*
 * Generic deallocation
 */ 
void kfree (void* addr);

/*
 * Size-bount deallocation
 */
void kfree_sized(void* addr, size_t allocation_size);

/*
 * Makes sure the heap can fit at least this much bytes
 */
void kheap_ensure_size(size_t size);

void* allocate_heap_memory(generic_heap_t* heap, size_t size);
void deallocate_heap_memory(generic_heap_t* heap, void* addr, size_t size);

/*
 * Allocate and deallocate memory in the current threads heap
 */
void* allocate_memory(size_t size);
void deallocate_memory(void* addr, size_t size);

void kdebug();

#endif //__ANIVA_HEAP_CORE__
