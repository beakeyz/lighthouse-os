#ifndef __KMALLOC__
#define __KMALLOC__
#include <libc/stddef.h>
#include "arch/x86/mem/kmem_bitmap.h"

// TODO: spinlock :clown:
// Node or Slab (or Chunk)?

#define CHUNK_SIZE 64

typedef struct {
    size_t total_chunks;
    size_t allocated_chunks;
    uint8_t* heap_addr;
} kmalloc_data_t;

void init_kmalloc (kmem_bitmap_t* bitmap);

void* kmalloc (size_t len);
void* kfree (void* addr, size_t len);
bool _try_heap_expand (size_t len);

#endif // !__KMALLOC__
