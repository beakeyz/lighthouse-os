#include "heap.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "malloc.h"
#include "mem/kmem_manager.h"
#include <sched/scheduler.h>
#include "mem/zalloc.h"
#include "sync/mutex.h"
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
void init_kheap() {

  heap_node_t* initial_node;
  heap_node_buffer_t* initial_buffer;

  /* Initialize the kernel allocator */
  __kernel_allocator.m_flags = NULL;
  __kernel_allocator.m_free_size = INITIAL_HEAP_TOTAL_SIZE;
  __kernel_allocator.m_used_size = NULL;

  /* Pretty good dummy for the parent dir lmao */
  __kernel_allocator.m_parent_dir = (page_dir_t) {
    .m_root = nullptr,
    .m_kernel_low = HIGH_MAP_BASE,
    .m_kernel_high = 0xFFFFFFFFFFFFFFFF,
  };

  /* Initialize the buffer */
  initial_buffer = (heap_node_buffer_t*)&init_kmalloc_mem;

  /* Set up the buffer */
  initial_buffer->m_node_count = 1;
  initial_buffer->m_buffer_size = INITIAL_HEAP_TOTAL_SIZE;
  initial_buffer->m_last_free_node = nullptr;
  initial_buffer->m_next = nullptr;

  /* Set up the initial node */
  initial_node = initial_buffer->m_start_node;

  initial_node->flags = NULL;
  initial_node->identifier = MALLOC_NODE_IDENTIFIER;
  initial_node->size = INITIAL_HEAP_TOTAL_SIZE - sizeof(heap_node_buffer_t);
  initial_node->prev = nullptr;
  initial_node->next = nullptr;

  /* Give the buffer to the allocator */
  __kernel_allocator.m_buffers = initial_buffer;
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
void* kmalloc (size_t len) 
{
  return memory_allocate(&__kernel_allocator, len);
}

// our kernel free impl
void kfree (void* addr) 
{
  memory_deallocate(&__kernel_allocator, addr);
}

void kfree_sized(void* addr, size_t allocation_size) 
{
  memory_sized_deallocate(&__kernel_allocator, addr, allocation_size);
}

void kheap_ensure_size(size_t size) 
{
  memory_try_heap_expand(&__kernel_allocator, size);
}
