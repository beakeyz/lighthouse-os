#include "heap.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/string.h"
#include "malloc.h"
#include "mem/base_allocator.h"
#include "mem/kmem_manager.h"
#include <sched/scheduler.h>
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include <entry/entry.h>

// Let's just give our kernel a shitload of initial heapmemory =)
#define INITIAL_HEAP_SIZE ALIGN_UP(128 * Kib, SMALL_PAGE_SIZE) // IN BYTES
#define INITIAL_KHEAP_VBASE (EARLY_KERNEL_HEAP_BASE) 

// fuk yea
// reserve enough, so we can use kmem_manager later to expand the heap
SECTION(".heap") static uint8_t init_kmalloc_mem[INITIAL_HEAP_SIZE] __attribute__((aligned(SMALL_PAGE_SIZE)));

// TODO: structure
static memory_allocator_t s_kernel_allocator;
static generic_heap_t s_generic_kernel_heap;

void init_kheap() {

  s_kernel_allocator.m_heap_start_node = (void*)kmem_ensure_high_mapping((uintptr_t)&init_kmalloc_mem[0]);

  // start node
  s_kernel_allocator.m_heap_start_node->identifier = MALLOC_NODE_IDENTIFIER;
  s_kernel_allocator.m_heap_start_node->size = INITIAL_HEAP_SIZE;
  s_kernel_allocator.m_heap_start_node->flags = 0;
  s_kernel_allocator.m_heap_start_node->next = NULL;
  s_kernel_allocator.m_heap_start_node->prev = NULL;

  // bottom node fix-up
  //s_kernel_allocator.m_heap_bottom_node = s_kernel_allocator.m_heap_start_node;

  // heap data v1
  s_kernel_allocator.m_heap = &s_generic_kernel_heap;
  s_kernel_allocator.m_used_size = 0;
  s_kernel_allocator.m_free_size = INITIAL_HEAP_SIZE;
  s_kernel_allocator.m_nodes_count = 1;

  // heap data v2
  s_generic_kernel_heap.m_current_total_size = INITIAL_HEAP_SIZE;
  s_generic_kernel_heap.m_flags = GHEAP_KERNEL;
  s_generic_kernel_heap.m_virtual_base = (vaddr_t) INITIAL_KHEAP_VBASE;
  s_generic_kernel_heap.m_physical_base = (paddr_t) &init_kmalloc_mem[0];

  // generic heap
  s_generic_kernel_heap.m_parent_heap = &s_kernel_allocator;
  s_generic_kernel_heap.f_deallocate = (HEAP_DEALLOCATE) malloc_deallocate;
  s_generic_kernel_heap.f_allocate = (HEAP_ALLOCATE) malloc_allocate;
  s_generic_kernel_heap.f_sized_deallocate = (HEAP_SIZED_DEALLOCATE) malloc_sized_deallocate;
  s_generic_kernel_heap.f_debug = nullptr;
  s_generic_kernel_heap.f_expand = (HEAP_EXPAND) malloc_try_heap_expand;
  s_generic_kernel_heap.f_on_expand_enable = (HEAP_ON_EXPAND_ENABLE) malloc_on_heap_expand_enable;

  init_zalloc();

  // TODO: show debug message?
}

void kheap_enable_expand() {
  enable_heap_expantion(s_kernel_allocator.m_heap);
}

// our kernel malloc impl
void* kmalloc (size_t len) {
  return s_kernel_allocator.m_heap->f_allocate(&s_kernel_allocator, len);
}

// our kernel free impl
void kfree (void* addr) {
  s_kernel_allocator.m_heap->f_deallocate(&s_kernel_allocator, addr);
}

void kfree_sized(void* addr, size_t allocation_size) {
  s_kernel_allocator.m_heap->f_sized_deallocate(&s_kernel_allocator, addr, allocation_size);
}

void kheap_ensure_size(size_t size) {
  s_kernel_allocator.m_heap->f_expand(&s_kernel_allocator, size);
}

void kdebug() {
  quick_print_node_sizes(&s_kernel_allocator);
}

void* allocate_heap_memory(generic_heap_t* heap, size_t size) {
  if (!heap || !heap->m_lock)
    return nullptr;

  mutex_lock(heap->m_lock);

  void* result = heap->f_allocate(heap->m_parent_heap, size);

  mutex_unlock(heap->m_lock);

  return result;
}

void deallocate_heap_memory(generic_heap_t* heap, void* addr, size_t size) {
  if (!heap || !heap->m_lock)
    return;

  mutex_lock(heap->m_lock);

  heap->f_sized_deallocate(heap->m_parent_heap, addr, size);

  mutex_unlock(heap->m_lock);
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
