#ifndef __ANIVA_HEAP_CORE__
#define __ANIVA_HEAP_CORE__
#include <libk/stddef.h>
#include <libk/error.h>

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

void kdebug();

#endif //__ANIVA_HEAP_CORE__
