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

void* kmalloc (size_t len);
void kfree (void* addr);
void kfree_sized(void* addr, size_t allocation_size);

#endif //__ANIVA_HEAP_CORE__
