// For now this will be a super simple pool allocator. In order to increase speed and
// flexibility, we will add a slab allocator in the future for smaller allocations.

#ifndef __ANIVA_MALLOC__
#define __ANIVA_MALLOC__
#include "libk/flow/error.h"
#include "mem/page_dir.h"
#include <libk/stddef.h>

#define MALLOC_NODE_FLAG_USED 0x0000000000000001ULL
/* Bit to check validity of the node */
#define MALLOC_NODE_FLAG_PARITY 0x0000000000000002ULL
#define MALLOC_NODE_FLAG_READONLY 0x0000000000000004ULL

#define MALLOC_NODE_FLAGS_MASK 0x0000000000000007ULL
#define MALLOC_NODE_SIZE_MASK 0xfffffffffffffff8ULL

// TODO: spinlock :clown:

// FIXME: this design may have some security issues =(
// all heap nodes should be alligned to a page
typedef struct heap_node {
    // size of this entry in bytes
    size_t attr;
    // duh
    struct heap_node* next;
    // duh 2x
    struct heap_node* prev;

    uint8_t data[];
} heap_node_t;

typedef struct heap_node_buffer {

    /* Amount of nodes that are present here */
    size_t m_node_count;

    /* Total size that the buffer takes up */
    size_t m_buffer_size;

    /* Link through all the buffers with this */
    struct heap_node_buffer* m_next;

    /* This list is kinda scetchy, since we cant index it regularly, because not every node has the same size */
    heap_node_t m_start_node[];
} heap_node_buffer_t;

typedef struct memory_allocator {

    // this node it the node that lives at the absolute bottom,
    // and thus is vulnerable to merging after an expansion
    // NOTE: we need a good way to keep track of this badboy
    // heap_node_t *m_heap_bottom_node;

    uint32_t m_flags;
    uint32_t m_buffer_count;

    page_dir_t m_parent_dir;

    heap_node_buffer_t* m_buffers;

    size_t m_free_size;
    size_t m_used_size;
} memory_allocator_t;

int malloc_heap_init(memory_allocator_t* allocator, void* buffer, size_t size, uint32_t flags);
void malloc_heap_dump(memory_allocator_t* allocator);

memory_allocator_t* create_malloc_heap(size_t size, vaddr_t virtual_base, uintptr_t flags);
memory_allocator_t* create_malloc_heap_ex(page_dir_t* dir, size_t size, vaddr_t virtual_base, uintptr_t flags);

void* memory_allocate(memory_allocator_t* allocator, size_t bytes);
void memory_sized_deallocate(memory_allocator_t* allocator, void* addr, size_t allocation_size);
void memory_deallocate(memory_allocator_t* allocator, void* addr);

ANIVA_STATUS memory_try_heap_expand(memory_allocator_t* allocator, size_t new_size);

// TODO: remove
void quick_print_node_sizes(memory_allocator_t* allocator);

heap_node_t* memory_get_heapnode_at(heap_node_buffer_t* buffer, uint32_t index);

// TODO: add a wrapper for userspace?
#endif // !__ANIVA_MALLOC__
