#ifndef __ANIVA_HEAP_CORE__
#define __ANIVA_HEAP_CORE__
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

/*
 * Allocate and deallocate memory in the current threads heap
void* allocate_memory(size_t size);
void deallocate_memory(void* addr, size_t size);
*/

#endif //__ANIVA_HEAP_CORE__
