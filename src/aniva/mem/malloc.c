#include "malloc.h"
#include "mem/kmem_manager.h"
#include "base_allocator.h"
#include "heap.h"
#include <libk/string.h>
#include <dev/debug/serial.h>
#include <libk/stddef.h>
#include <libk/error.h>

static heap_node_t* create_initial_heap_node(memory_allocator_t *allocator, void* start_address);
static heap_node_t* split_node (memory_allocator_t * allocator, heap_node_t* ptr, size_t size);
static heap_node_t* merge_node_with_next (memory_allocator_t * allocator, heap_node_t* ptr);
static heap_node_t* merge_node_with_prev (memory_allocator_t * allocator, heap_node_t* ptr);
static heap_node_t* merge_nodes (memory_allocator_t * allocator, heap_node_t* ptr1, heap_node_t* ptr2);
static heap_node_t* try_merge (memory_allocator_t * allocator, heap_node_t* node);
static bool can_merge (heap_node_t* node1, heap_node_t* node2);
static bool has_flag(heap_node_t* node, uint8_t flag);

memory_allocator_t *create_malloc_heap(size_t size, vaddr_t virtual_base, uintptr_t flags) {
  // create a heap object on the initial kernel heap :clown:
  memory_allocator_t *heap = kmalloc(sizeof(memory_allocator_t));

  // TODO: also call the on_expand_enable function here
  flags |= GHEAP_EXPANDABLE;

  heap->m_heap = initialize_generic_heap(nullptr, virtual_base, size, flags);
  heap->m_heap->f_allocate = (HEAP_ALLOCATE) malloc_allocate;
  heap->m_heap->f_deallocate = (HEAP_DEALLOCATE) malloc_deallocate;
  heap->m_heap->f_sized_deallocate = (HEAP_SIZED_DEALLOCATE) malloc_sized_deallocate;
  heap->m_heap->f_expand = (HEAP_EXPAND) malloc_try_heap_expand;
  heap->m_heap->f_debug = (HEAP_GENERAL_DEBUG) quick_print_node_sizes;
  heap->m_heap->f_on_expand_enable = (HEAP_ON_EXPAND_ENABLE) malloc_on_heap_expand_enable;
  heap->m_heap->m_parent_heap = heap;

  heap->m_heap_start_node = create_initial_heap_node(heap, (void *) heap->m_heap->m_virtual_base);
  heap->m_heap_bottom_node = heap->m_heap_start_node;
  heap->m_free_size = size;
  heap->m_used_size = 0;
  heap->m_nodes_count = 1;

  return heap;
}

// FIXME: probably should not directly print on a packed struct...
void quick_print_node_sizes(memory_allocator_t* allocator) {
  heap_node_t* node = allocator->m_heap_start_node;
  uintptr_t index = 0;
  while (node) {
    index++;
    
    print("node ");
    print(to_string(index));
    print(": ");
    if (!has_flag(node, MALLOC_FLAGS_USED)) {
      print("(free) ");
    }
    if (node->next == nullptr) {
      print("(no next) ");
    }
    print("true size: ");
    size_t size = node->size;
    print(to_string(size));

    print(" size: ");
    println(to_string(size - sizeof(heap_node_t)));

    node = node->next;
  }
  print("Amount of nodes found: ");
  println(to_string(index));
  print("Free heap (bytes): ");
  println(to_string(allocator->m_free_size));
  print("Used heap (bytes): ");
  println(to_string(allocator->m_used_size));
}

// kmalloc is going to split a node and then return the address of the newly created node + its size to get
// a pointer to the data
void* malloc_allocate(memory_allocator_t * allocator, size_t bytes) {
  println("MALLOC");

  heap_node_t* node = allocator->m_heap_start_node;
  while (node) {
    // TODO: should we also allow allocation when len is equal to the nodesize - structsize?

    if (node->size - sizeof(heap_node_t) > bytes && !has_flag(node, MALLOC_FLAGS_USED)) {
      // yay, our node works =D

      // now split off a node of the correct size
      heap_node_t* new_node = split_node(allocator, node, bytes);
      if (new_node == nullptr) {
        node = node->next;
        continue;
      }

      new_node->flags |= MALLOC_FLAGS_USED;
      // for sanity
      new_node->identifier = MALLOC_NODE_IDENTIFIER;
      
      allocator->m_free_size -= new_node->size;
      allocator->m_used_size += new_node->size;

      // TODO: edit global shit
      return (void*) new_node + sizeof(heap_node_t);
    }

    // break early, so we can pass the node to the expand func
    if (!node->next) {
      println("needs: ");
      println(to_string(bytes));
      println("has: ");
      println(to_string(allocator->m_free_size));
      break;
    }
    node = node->next;
  }

  // TODO: implement dynamic expanding
  if (malloc_try_heap_expand(allocator, SMALL_PAGE_SIZE)) {
      return malloc_allocate(allocator, bytes);
  }

  kernel_panic("FAILED TO MALLOC KERNEL MEMORY");
  // yikes
  return NULL;
}

void malloc_sized_deallocate(memory_allocator_t* allocator, void* addr, size_t allocation_size) {
  if (addr == nullptr || allocator == nullptr) {
    return;
  }

  // first we'll check if we can do this easily
  heap_node_t* node = (heap_node_t*)((uintptr_t)addr - sizeof(heap_node_t));
  const size_t real_allocation_size = allocation_size + sizeof(heap_node_t);

  if (node->size != real_allocation_size) {
    return;
  }

  if (verify_identity(node)) {
    // we can free =D
    if (has_flag(node, MALLOC_FLAGS_USED)) {
      node->flags &= ~MALLOC_FLAGS_USED;
      allocator->m_used_size-=node->size;
      allocator->m_free_size+=node->size;
      // see if we can merge
      heap_node_t* n = try_merge(allocator, node);
      if (n == nullptr) {
        // FUCKKKKK
        println("unable to merge nodes =/");
        //memset(addr, 0, node->size - sizeof(heap_node_t));
        return;
      }

      if (n->next)
        try_merge(allocator, n->next);
      // FIXME: should we zero freed nodes?
    }
  } else {
    println("invalid identifier");
  }
}

void malloc_deallocate(memory_allocator_t* allocator, void* addr) {

  if (addr == nullptr || allocator == nullptr) {
    return;
  }

  // first we'll check if we can do this easily
  heap_node_t* node = (heap_node_t*)((uintptr_t)addr - sizeof(heap_node_t));
  if (verify_identity(node)) {
    // we can free =D
    if (has_flag(node, MALLOC_FLAGS_USED)) {
      node->flags &= ~MALLOC_FLAGS_USED;
      allocator->m_used_size-=node->size;
      allocator->m_free_size+=node->size;
      // see if we can merge
      heap_node_t* n = try_merge(allocator, node);
      if (n == nullptr) {
        //println("unable to merge nodes =/");
        return;
      }

      if (n->next)
        try_merge(allocator, n->next);
      // FIXME: should we zero freed nodes?
    }
  } else {
    println("invalid identifier");
  }
}

// just expand the heap by one 4KB page
ANIVA_STATUS malloc_try_heap_expand(memory_allocator_t *allocator, size_t extra_size) {
  // TODO: implement non-kernel heaps
  println("HEAP EXPANTION");
  ASSERT_MSG((allocator->m_heap->m_flags & GHEAP_EXPANDABLE) && (allocator->m_heap->m_flags & GHEAP_KERNEL), "Heap is not ready to be expanded!");

  const vaddr_t new_mapping_base = allocator->m_heap->m_virtual_base + allocator->m_heap->m_current_total_size;

  println(to_string(new_mapping_base));
  println(to_string(ALIGN_UP(new_mapping_base, SMALL_PAGE_SIZE)));

  ASSERT_MSG(ALIGN_UP(new_mapping_base, SMALL_PAGE_SIZE) == new_mapping_base, "Heap was misaligned while expanding!");

  extra_size = ALIGN_UP(extra_size, SMALL_PAGE_SIZE);

  // request a page from kmem_manager
  ErrorOrPtr result = kmem_kernel_map_and_alloc_range(extra_size, new_mapping_base, KMEM_CUSTOMFLAG_GET_MAKE, 0);
  if (result.m_status != ANIVA_SUCCESS) {
    return ANIVA_FAIL;
  }

  allocator->m_heap->m_current_total_size += extra_size;
  println(to_string(extra_size));

  quick_print_node_sizes(allocator);

  heap_node_t* new_node = (heap_node_t*)result.m_ptr;
  if (new_node) {
    new_node->size = extra_size;
    new_node->identifier = MALLOC_NODE_IDENTIFIER;
    new_node->flags = 0;
    new_node->next = 0;
    new_node->prev = allocator->m_heap_bottom_node;
    allocator->m_heap_bottom_node->next = new_node;

    println(to_string(kmem_get_page(nullptr, (uintptr_t)allocator->m_heap_bottom_node, 0)->raw_bits));
    if (try_merge(allocator, allocator->m_heap_bottom_node) != nullptr) {
      return ANIVA_SUCCESS;
    }
  }
  return ANIVA_FAIL;
}

void malloc_on_heap_expand_enable(memory_allocator_t* allocator) {

  println("Trying to expand heap!");
  const vaddr_t heap_vbase = allocator->m_heap->m_virtual_base;
  const size_t heap_current_size = allocator->m_heap->m_current_total_size;

  ASSERT_MSG(ALIGN_UP(heap_vbase, SMALL_PAGE_SIZE) == heap_vbase, "heap_vbase is not aligned!");
  ASSERT_MSG(ALIGN_UP(heap_current_size, SMALL_PAGE_SIZE) == heap_current_size, "heap_current_size is not aligned!");

  bool result = kmem_map_range(nullptr, heap_vbase, (uintptr_t)allocator->m_heap_start_node, heap_current_size / SMALL_PAGE_SIZE, KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE, 0);

  if (!result) {
    kernel_panic("Failed to enable expantion!");
  }
}

heap_node_t* split_node(memory_allocator_t * allocator, heap_node_t *ptr, size_t size) {
  // trying to split a node into a bigger size than it itself is xD
  if (ptr->size <= size) {
    // this is just dumb
    //println("[kmalloc:split_node] size is invalid");
    return nullptr;
  }

  const size_t node_free_size = ptr->size - sizeof(heap_node_t);
  const size_t extra_size = size + sizeof(heap_node_t);
  if (node_free_size < extra_size) {
    //println("[kmalloc:split_node] node too small!");
    return nullptr;
  }
  
  
  if (has_flag(ptr, MALLOC_FLAGS_USED)) {
    // a node that's completely in use, should not be split
    //println("[kmalloc:split_node] tried to split a node that's in use");
    return nullptr;
  }

  // TODO: handle this correctly
  ASSERT_MSG(verify_identity(ptr), "Tried to split a unidentified node!");

  // the new node for the allocation
  heap_node_t* new_first_node = ptr;
  // the old free node
  heap_node_t* new_second_node = (heap_node_t*)((uintptr_t)ptr + size + sizeof(heap_node_t));
  new_first_node->identifier = MALLOC_NODE_IDENTIFIER;
  new_second_node->identifier = MALLOC_NODE_IDENTIFIER;
  // now copy the data of the (big) free node over to its smaller counterpart
  new_second_node->size = new_first_node->size - size - sizeof(heap_node_t);
  // This should just be free, but we'll just copy it over anyway
  new_second_node->flags = new_first_node->flags;

  // pointer resolve

  new_second_node->prev = new_first_node;
  new_second_node->next = new_first_node->next;

  // now init the data of our new used node
  new_first_node->next = new_second_node;

  // TODO: these two should prob be asserts
  new_first_node->prev = ptr->prev;
  new_first_node->flags = ptr->flags;
  new_first_node->size = size + sizeof(heap_node_t);

  allocator->m_nodes_count++;

  return new_first_node;
}

bool can_merge(heap_node_t *node1, heap_node_t *node2) {
  if (node2 == nullptr || node1 == nullptr)
    return false;

  // Only free nodes can merge
  if (!has_flag(node1, MALLOC_FLAGS_USED) && !has_flag(node2, MALLOC_FLAGS_USED)) {
    if ((node1->prev == node2 && node2->next == node1) || (node1->next == node2 && node2->prev == node1)) {
      return true;
    } 
  }
  return false;
}

// we check for mergeability again, just for sanity =/
// I'd love to make this more efficient (and readable), but I'm also too lazy xD
heap_node_t* merge_node_with_next (memory_allocator_t * allocator, heap_node_t* ptr) {
  if (can_merge(ptr, ptr->next)) {
    ptr->size += ptr->next->size;
    heap_node_t* next_next = ptr->next->next;
    //memset(ptr->next, 0, ptr->next->size);

    next_next->prev = ptr;
    ptr->next = next_next;

    allocator->m_nodes_count--;
    return ptr;
  }
  return nullptr;
}
heap_node_t* merge_node_with_prev (memory_allocator_t * allocator, heap_node_t* ptr) {
  if (can_merge(ptr, ptr->prev)) {
    heap_node_t* prev = ptr->prev;

    prev->size += ptr->size;
    prev->next = ptr->next;
    //memset(ptr, 0, ptr->size);
    allocator->m_nodes_count--;
    return prev;
  }
  return nullptr;
}
heap_node_t* merge_nodes (memory_allocator_t * allocator, heap_node_t* ptr1, heap_node_t* ptr2) {
  if (can_merge(ptr1, ptr2)) {
    // Because they passed the merge check we can do this =D
    // sm -> ptr1 -> ptr2 -> sm
    if (ptr1->next == ptr2) {
        return merge_node_with_next(allocator,ptr1);
    }
    // ptr2 -> ptr1 -> sm
    return merge_node_with_prev(allocator,ptr1);
  }
  return nullptr;
}

heap_node_t* try_merge(memory_allocator_t * allocator, heap_node_t *node) {
  heap_node_t* merged_node = merge_nodes(allocator,node, node->next);
  if (merged_node == nullptr) {
    merged_node = merge_nodes(allocator,node, node->prev);
  }

  if (merged_node) {
    try_merge(allocator,merged_node);
  }

  return merged_node;
}

// NOTE: when the identifier is not valid, we should ideally view the heap as corrupted and either
// purge and reinit, or just panic and die
bool verify_identity(heap_node_t *node) {
  return (node->identifier == MALLOC_NODE_IDENTIFIER);
}

bool has_flag(heap_node_t* node, uint8_t flag) {
  return (node->flags & flag) != 0;
}

static heap_node_t *create_initial_heap_node(memory_allocator_t *allocator, void* start_address) {
  heap_node_t *ret = (heap_node_t*)start_address;

  ret->next = nullptr;
  ret->prev = nullptr;
  ret->size = allocator->m_heap->m_current_total_size;
  ret->flags = 0;
  ret->identifier = MALLOC_NODE_IDENTIFIER;
  return ret;
}
