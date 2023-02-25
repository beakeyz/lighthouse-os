// For now this will be a super simple pool allocator. In order to increase speed and 
// flexibility, we will add a slab allocator in the future for smaller allocations.

#ifndef __ANIVA_MALLOC__
#define __ANIVA_MALLOC__
#include "dev/debug/serial.h"
#include "base_allocator.h"
#include <libk/stddef.h>

typedef enum MALLOC_NODE_FLAGS {
  MALLOC_FLAGS_USED = (1 << 0),
  MALLOC_FLAGS_READONLY = (1 << 1)
} MALLOC_NODE_FLAGS_t;

#define MALLOC_NODE_IDENTIFIER 0xF0CEDA22

// TODO: spinlock :clown:

// FIXME: this design may have some security issues =(
// all heap nodes should be alligned to a page
typedef struct heap_node {
  // size of this entry in bytes
  size_t size;
  // used to validate a node pointer
  uint32_t identifier;
  // flags for this block
  uint8_t flags;
  // duh
  struct heap_node* next;
  // duh 2x
  struct heap_node* prev;
  // plz no padding ;-;
} __attribute__((packed)) heap_node_t;

typedef struct memory_allocator {
  generic_heap_t *m_heap;

  // initial node this heap has to work with
  heap_node_t* m_heap_start_node;

  // this node it the node that lives at the absolute bottom,
  // and thus is vulnerable to merging after an expansion
  heap_node_t *m_heap_bottom_node;

  size_t m_nodes_count;
  size_t m_free_size;
  size_t m_used_size;
} memory_allocator_t;

memory_allocator_t *create_malloc_heap(size_t size, vaddr_t virtual_base, uintptr_t flags);

void* malloc_allocate(memory_allocator_t * allocator, size_t bytes);

void malloc_sized_deallocate(memory_allocator_t* allocator, void* addr, size_t allocation_size);

void malloc_deallocate(memory_allocator_t* allocator, void* addr);

/*
 * check the identifier of a node to confirm that is in fact a
 * node that we use
 * TODO: make this identifier dynamic and (perhaps) bound to
 * the heap that it belongs to
 */
bool verify_identity (heap_node_t* node);


// TODO: remove
void quick_print_node_sizes (memory_allocator_t* allocator);

void enable_heap_expantion ();
void disable_heap_expantion ();

// TODO: add a wrapper for userspace?
#endif // !__ANIVA_MALLOC__
