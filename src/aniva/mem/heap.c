#include "heap.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "malloc.h"
#include "mem/kmem_manager.h"
#include <sched/scheduler.h>
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include <entry/entry.h>

// Let's just give our kernel a shitload of initial heapmemory =)
#define INITIAL_HEAP_TOTAL_SIZE (ALIGN_UP(ALIGN_UP(128 * Kib, SMALL_PAGE_SIZE) + sizeof(heap_node_buffer_t), SMALL_PAGE_SIZE)) // IN BYTES
#define INITIAL_KHEAP_VBASE (EARLY_KERNEL_HEAP_BASE) 

// fuk yea
// reserve enough, so we can use kmem_manager later to expand the heap
SECTION(".heap") static uint8_t init_kmalloc_mem[INITIAL_HEAP_TOTAL_SIZE] __attribute__((aligned(SMALL_PAGE_SIZE)));

// TODO: structure
static memory_allocator_t s_kernel_allocator;

void init_kheap() {

  heap_node_t* initial_node;
  heap_node_buffer_t* initial_buffer;

  /* Initialize the kernel allocator */
  s_kernel_allocator.m_flags = NULL;
  s_kernel_allocator.m_free_size = INITIAL_HEAP_TOTAL_SIZE;
  s_kernel_allocator.m_used_size = NULL;

  /* Pretty good dummy for the parent dir lmao */
  s_kernel_allocator.m_parent_dir = (page_dir_t) {
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
  s_kernel_allocator.m_buffers = initial_buffer;

}

/**
 * TODO: either fix memory allocator, or migrate the entire kernel to use kfree_sized
 **/

// our kernel malloc impl
void* kmalloc (size_t len) {
  return memory_allocate(&s_kernel_allocator, len);
  //return kzalloc(len);
}

// our kernel free impl
void kfree (void* addr) {
  memory_deallocate(&s_kernel_allocator, addr);
  //kernel_panic("TODO: phase out memory allocator (it sucks mega balls and needs a big fix)");
}

void kfree_sized(void* addr, size_t allocation_size) {
  memory_sized_deallocate(&s_kernel_allocator, addr, allocation_size);
  //kzfree(addr, allocation_size);
}

void kheap_ensure_size(size_t size) {
  memory_try_heap_expand(&s_kernel_allocator, size);
}

void* allocate_memory(size_t size) {
  //generic_heap_t* heap = get_current_proc()->m_heap;

  //return allocate_heap_memory(heap, size);
  kernel_panic("TODO: implement allocate_memory");
}

void deallocate_memory(void* addr, size_t size) {
  //generic_heap_t* heap = get_current_proc()->m_heap;

  //deallocate_heap_memory(heap, addr, size);
  kernel_panic("TODO: implement deallocate_memory");
}
