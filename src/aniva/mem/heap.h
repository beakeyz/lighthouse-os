#ifndef __ANIVA_HEAP_CORE__
#define __ANIVA_HEAP_CORE__
#include "mem/malloc.h"
#include <libk/stddef.h>
#include <libk/flow/error.h>

#define GHEAP_READONLY      (1 << 0)
#define GHEAP_KERNEL        (1 << 1)
#define GHEAP_EXPANDABLE    (1 << 2)
#define GHEAP_ZEROED        (1 << 3)
#define GHEAP_NOBLOCK       (1 << 4)

/*
 * initialize the first kernel heap manually. after this
 * heaps can be dynamically and automatically created
 */
void init_kheap();
void debug_kheap();

void* kmalloc (size_t len);
void kfree (void* addr);

int kheap_copy_main_allocator(memory_allocator_t* alloc);

#endif //__ANIVA_HEAP_CORE__
