#include "malloc.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "heap.h"
#include "sched/scheduler.h"
#include "system/asm_specifics.h"
#include <libk/string.h>
#include <dev/debug/serial.h>
#include <libk/stddef.h>
#include <libk/flow/error.h>

/*
 * check the identifier of a node to confirm that is in fact a
 * node that we use
 * TODO: make this identifier dynamic and (perhaps) bound to
 * the heap that it belongs to
 * NOTE: when the identifier is not valid, we should ideally view the heap as corrupted and either
 * purge and reinit, or just panic and die
 */
static bool verify_identity(heap_node_t *node) {
  return (node->identifier == MALLOC_NODE_IDENTIFIER);
}

static bool has_flag(heap_node_t* node, uint8_t flag) {
  return (node->flags & flag) == flag;
}

static heap_node_t* split_node(heap_node_buffer_t* buffer, heap_node_t *ptr, size_t size) {
  // trying to split a node into a bigger size than it itself is xD
  if (ptr->size <= size) {
    // this is just dumb
    println("[kmalloc:split_node] size is invalid");
    return nullptr;
  }

  const size_t node_free_size = ptr->size - sizeof(heap_node_t);
  const size_t extra_size = size + sizeof(heap_node_t);
  if (node_free_size < extra_size) {
    //println("[kmalloc:split_node] node too small!");
    return nullptr;
  }
  
  if (has_flag(ptr, MALLOC_NODE_FLAG_USED)) {
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

  new_first_node->size = size + sizeof(heap_node_t);

  buffer->m_node_count++;

  return new_first_node;
}

static bool can_merge(heap_node_t *node1, heap_node_t *node2) {
  if (node2 == nullptr || node1 == nullptr)
    return false;


  // Only free nodes can merge
  if (!has_flag(node1, MALLOC_NODE_FLAG_USED) && !has_flag(node2, MALLOC_NODE_FLAG_USED)) {
    if ((node1->prev != nullptr && node1->prev == node2 && node2->next != nullptr && node2->next == node1) ||
        (node1->next != nullptr && node1->next == node2 && node2->prev != nullptr && node2->prev == node1)) {
      return true;
    } 
  }
  return false;
}

// we check for mergeability again, just for sanity =/
// I'd love to make this more efficient (and readable), but I'm also too lazy xD
static heap_node_t* merge_node_with_next (heap_node_buffer_t* buffer, heap_node_t* ptr) {
  if (can_merge(ptr, ptr->next)) {
    ptr->size += ptr->next->size;
    heap_node_t* next_next = ptr->next->next;
    memset(ptr->next, 0, sizeof(heap_node_t));

    if (next_next) {
      next_next->prev = ptr;
    } 
    ptr->next = next_next;

    buffer->m_node_count--;
    return ptr;
  }
  return nullptr;
}

static heap_node_t* merge_node_with_prev (heap_node_buffer_t* buffer, heap_node_t* ptr) {
  if (can_merge(ptr, ptr->prev)) {
    heap_node_t* prev = ptr->prev;

    prev->size += ptr->size;

    ptr->next->prev = prev;
    prev->next = ptr->next;

    memset(ptr, 0, sizeof(heap_node_t));

    buffer->m_node_count--;

    return prev;
  }
  return nullptr;
}

static heap_node_t* merge_nodes (heap_node_buffer_t* buffer, heap_node_t* ptr1, heap_node_t* ptr2) {
  if (can_merge(ptr1, ptr2)) {
    // Because they passed the merge check we can do this =D
    // sm -> ptr1 -> ptr2 -> sm
    
    if (ptr1->next == ptr2) {
      return merge_node_with_next(buffer,ptr1);
    }
    // ptr2 -> ptr1 -> sm
    return merge_node_with_prev(buffer,ptr1);
  }
  return nullptr;
}

static ErrorOrPtr try_merge(heap_node_buffer_t* buffer, heap_node_t *node) {
  ErrorOrPtr result = Error();
  heap_node_t* merged_node;

  /* TODO: loop limit */
  while (true) {
    merged_node = merge_nodes(buffer, node, node->prev);

    if (!merged_node)
      merged_node = merge_nodes(buffer, node, node->next);

    if (!merged_node)
      break;

    result = Success((vaddr_t)merged_node);
  }

  return result;
}

static inline void allocator_add_free(memory_allocator_t* allocator, size_t size)
{
  //print("Kmalloc: +");
  //println(to_string(size));
  allocator->m_free_size += size;
  allocator->m_used_size -= size;
}

static inline void allocator_add_used(memory_allocator_t* allocator, size_t size)
{
  //print("Kmalloc: -");
  //println(to_string(size));
  allocator->m_used_size += size;
  allocator->m_free_size -= size;
}

#define MEM_ALLOC_DEFAULT_BUFFERSIZE    (4 * Mib)
#define MEM_ALLOC_MIN_BUFFERSIZE        (64 * Kib) /* 16 Page minimum */

#define TO_NODE(addr) (heap_node_t*)((uintptr_t)addr - sizeof(heap_node_t))

/*
 * Creates a buffer and links it into the allocator
 */
static heap_node_buffer_t* create_heap_node_buffer(memory_allocator_t* allocator, size_t* size)
{
  size_t total_buffer_size;
  size_t data_size;
  heap_node_t* start_node;
  heap_node_buffer_t* ret;
  ErrorOrPtr result;

  if (!allocator || !size || !(*size))
    return nullptr;

  if (*size < MEM_ALLOC_MIN_BUFFERSIZE)
    *size = MEM_ALLOC_MIN_BUFFERSIZE;

  total_buffer_size = ALIGN_UP(*size + sizeof(heap_node_buffer_t), SMALL_PAGE_SIZE);

  result = __kmem_alloc_range(
        allocator->m_parent_dir.m_root,
        nullptr,
        KERNEL_MAP_BASE,
        total_buffer_size,
        NULL,
        KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL
        );

  if (IsError(result))
    return nullptr;

  /* FIXME: is this allocation corret? */
  ret = (heap_node_buffer_t*)Release(result);

  /* Total size of the available data */
  data_size = total_buffer_size - sizeof(heap_node_buffer_t);

  memset(ret, 0, sizeof(heap_node_buffer_t));

  ret->m_buffer_size = total_buffer_size;
  ret->m_node_count = 1;

  start_node = ret->m_start_node;

  memset(start_node, 0, sizeof(heap_node_t));

  /* The size of the node includes both the size of the node struct and the data region */
  start_node->size = data_size;
  start_node->next = nullptr;
  start_node->prev = nullptr;
  start_node->identifier = MALLOC_NODE_IDENTIFIER; 
  start_node->flags = NULL;

  /* Link into the allocator */
  ret->m_next = allocator->m_buffers;
  allocator->m_buffers = ret;

  allocator->m_buffer_count++;

  /*
   * The free_size field is a bit of a lie, since for every allocation we 
   * miss sizeof(heap_node_t) bytes lmao
   */
  allocator->m_free_size += data_size;

  return ret;
}

static void destroy_heap_node_buffer(memory_allocator_t* allocator, heap_node_buffer_t* buffer)
{
  heap_node_buffer_t* current_buffer;

  if (!allocator || !buffer)
    return;

  current_buffer = allocator->m_buffers;

  while (current_buffer) {

    /* Are we about to hit our target buffer? */
    if (current_buffer->m_next == buffer) {
      /* We are. Just skip it in the link and break */
      current_buffer->m_next = buffer->m_next;
      break;
    }

    current_buffer = current_buffer->m_next;

    /* TODO: graceful exit */
    ASSERT_MSG(current_buffer, "Failed to destroy heap_node_buffer! Did the allocator contain the buffer?");
  }

  Must(__kmem_dealloc(allocator->m_parent_dir.m_root, nullptr, (uintptr_t)buffer, buffer->m_buffer_size));
}

static bool heap_buffer_contains(heap_node_buffer_t* buffer, uintptr_t addr)
{
  const uintptr_t buffer_start = (uintptr_t)&buffer->m_start_node;
  return (addr >= buffer_start && addr < buffer_start + buffer->m_buffer_size);
}

heap_node_t* memory_get_heapnode_at(heap_node_buffer_t* buffer, uint32_t index)
{
  heap_node_t* ret;

  if (!buffer)
    return nullptr;

  if (!index)
    return buffer->m_start_node;

  ret = buffer->m_start_node;

  while (ret && index) {
    ret = ret->next;
    index--;
  }

  return ret;
}

static ErrorOrPtr heap_buffer_allocate_in(memory_allocator_t* allocator, heap_node_buffer_t* buffer, heap_node_t* node, size_t bytes)
{
  if (!allocator || !buffer || !bytes)
    return Error();

  while (node) {
    // TODO: should we also allow allocation when len is equal to the nodesize - structsize?

    /*
     * Perfect fit: yoink
     */
    if ((node->size - sizeof(heap_node_t)) == bytes && !has_flag(node, MALLOC_NODE_FLAG_USED)) {
      node->flags |= MALLOC_NODE_FLAG_USED;

      allocator_add_used(allocator, node->size);

      return Success((uintptr_t)node->data);
    } 
    
    if ((node->size - sizeof(heap_node_t)) > bytes && !has_flag(node, MALLOC_NODE_FLAG_USED)) {
      // yay, our node works =D

      // now split off a node of the correct size
      heap_node_t* new_node = split_node(buffer, node, bytes);

      if (!new_node)
        goto cycle;

      // for sanity
      new_node->identifier = MALLOC_NODE_IDENTIFIER;
      new_node->flags |= MALLOC_NODE_FLAG_USED;

      ASSERT_MSG((new_node->next->flags & MALLOC_NODE_FLAG_USED) == 0, "Next node was not free, something went wront while allocating memory!");

      buffer->m_last_free_node = new_node->next;
      
      allocator_add_used(allocator, new_node->size);

      // TODO: edit global shit
      return Success((uintptr_t)new_node->data);
    }

cycle:
    node = node->next;
  }

  /* Could not split off a node. We need a new buffer */
  return Error();
}

static ErrorOrPtr heap_buffer_allocate(memory_allocator_t* allocator, heap_node_buffer_t* buffer, size_t bytes)
{
  /* 
   * Just bruteforce it from the start 
   * FIXME: should we start from the end actually?
   */
  return heap_buffer_allocate_in(allocator, buffer, buffer->m_start_node, bytes);
}

static ErrorOrPtr heap_buffer_deallocate(memory_allocator_t* allocator, heap_node_buffer_t* buffer, void* addr)
{
  // first we'll check if we can do this easily
  ErrorOrPtr result;
  heap_node_t* node = TO_NODE(addr);

  /* Invalid address */
  if (!verify_identity(node))
    return Error();

  if (!has_flag(node, MALLOC_NODE_FLAG_USED))
    return Error();

  /* Wrong buffer */
  if (!heap_buffer_contains(buffer, (uintptr_t)node))
    return Error();

  allocator_add_free(allocator, node->size);

  result = try_merge(buffer, node);

  if (!IsError(result))
    node = (heap_node_t*)Release(result);

  node->flags &= ~MALLOC_NODE_FLAG_USED;

  // FIXME: should we zero freed nodes?
  return Success(0);
}

memory_allocator_t *create_malloc_heap(size_t size, vaddr_t virtual_base, uintptr_t flags) {

  kernel_panic("TODO: create_malloc_heap");
}

void destroy_malloc_heap(memory_allocator_t* allocator)
{
  heap_node_buffer_t* next;
  heap_node_buffer_t* buffer;

  buffer = allocator->m_buffers;

  while (buffer) {
    next = buffer->m_next;

    destroy_heap_node_buffer(allocator, buffer);

    buffer = next;
  }

  kernel_panic("TODO: implement destroy_malloc_heap");

}

// kmalloc is going to split a node and then return the address of the newly created node + its size to get
// a pointer to the data
// TODO: first check the bottom node to see if we can just 
// chuck this one at the end, otherwise loop over the list to 
// find an unused node
void* memory_allocate(memory_allocator_t * allocator, size_t bytes) {

  ErrorOrPtr ret;
  heap_node_buffer_t* current_buffer;

  if (!allocator || !bytes)
    return nullptr;

  current_buffer = allocator->m_buffers;

  /*
   * Loop over all the linked buffers and try to allocate in them
   */
  while (current_buffer) {

    ret = heap_buffer_allocate(allocator, current_buffer, bytes);

    if (!IsError(ret))
      return (void*)Release(ret);

    current_buffer = current_buffer->m_next;
  }

  /*
   * TODO: intelligently calculate a best new size for this buffer
   * (
   *    Perhaps by taking timestamps in between heap usages and trying to 
   *    determine what size is most used?
   * )
   */

  println("Malloc expantion!!!");

  heap_node_buffer_t* new_buffer;
  size_t size = MEM_ALLOC_DEFAULT_BUFFERSIZE;

  /* Could not allocate in any existing buffer. Try to create a new one */
  new_buffer = create_heap_node_buffer(allocator, &size);

  /* FUCKKK */
  if (!new_buffer)
    return nullptr;

  ret = heap_buffer_allocate(allocator, new_buffer, bytes);

  if (IsError(ret))
    return nullptr;

  return (void*)Release(ret);
}

void memory_sized_deallocate(memory_allocator_t* allocator, void* addr, size_t allocation_size) {
  kernel_panic("TODO: memory_sized_deallocate");
}

void memory_deallocate(memory_allocator_t* allocator, void* addr) {

  heap_node_buffer_t* current_buffer;

  if (!addr || !allocator)
    return;

  current_buffer = allocator->m_buffers;

  while (current_buffer) {

    if (!IsError(heap_buffer_deallocate(allocator, current_buffer, addr))) {
      break;
    }

    current_buffer = current_buffer->m_next;
  }

  //println("Could not find buffer to deallocate in!");
}

// just expand the heap by some page-aligned amount
ANIVA_STATUS memory_try_heap_expand (memory_allocator_t * allocator, size_t new_size) {
  return ANIVA_FAIL;
}
