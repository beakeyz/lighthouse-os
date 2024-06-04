#include "heap.h"
#include "malloc.h"
#include "mem/kmem_manager.h"
#include <sched/scheduler.h>
#include <entry/entry.h>

/*
 * FIXME: this REAAALY screws us good. Can we figure some way to work around the fact that we need this 
 * big of a heap to support systems with big memory profiles?
 */
#define INITIAL_HEAP_TOTAL_SIZE (ALIGN_UP(ALIGN_UP(32 * Kib, SMALL_PAGE_SIZE) + sizeof(heap_node_buffer_t), SMALL_PAGE_SIZE)) // IN BYTES
#define INITIAL_KHEAP_VBASE (EARLY_KERNEL_HEAP_BASE) 

/* TODO: GET FUCKING RID OF THIS BULLSHIT */
SECTION(".heap") static uint8_t init_kmalloc_mem[INITIAL_HEAP_TOTAL_SIZE] __attribute__((aligned(SMALL_PAGE_SIZE)));

static memory_allocator_t __kernel_allocator;

/*!
 * @brief Manually set up the kernel allocator
 *
 * Nothing to add here...
 */
void init_kheap()
{
  malloc_heap_init(&__kernel_allocator, init_kmalloc_mem, sizeof(init_kmalloc_mem), NULL);
}

void debug_kheap()
{
  malloc_heap_dump(&__kernel_allocator);
}

int kheap_copy_main_allocator(memory_allocator_t* alloc)
{
  if (!alloc)
    return -1;

  memcpy(alloc, &__kernel_allocator, sizeof(__kernel_allocator));
  return 0;
}

/**
 * TODO: either fix memory allocator, or migrate the entire kernel to use kfree_sized
 **/

// our kernel malloc impl
inline void* kmalloc (size_t len) 
{
  return memory_allocate(&__kernel_allocator, len);
}

// our kernel free impl
inline void kfree (void* addr) 
{
  return memory_deallocate(&__kernel_allocator, addr);
}
