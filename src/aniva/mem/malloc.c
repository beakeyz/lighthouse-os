#include "malloc.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include <dev/debug/serial.h>
#include <libk/flow/error.h>
#include <libk/stddef.h>
#include <libk/string.h>

static inline void heap_node_set_size(heap_node_t* node, size_t size)
{
    node->attr &= MALLOC_NODE_FLAGS_MASK;
    node->attr |= ALIGN_UP(size, 8);
}

static inline size_t heap_node_get_size(heap_node_t* node)
{
    return (node->attr & MALLOC_NODE_SIZE_MASK);
}

static inline bool heap_node_has_flag(heap_node_t* node, uint64_t flag)
{
    return (node->attr & flag) == flag;
}

/*!
 * @brief: Checks if a node is valid
 *
 * TODO: Implement
 */
// static inline bool heap_node_is_valid(heap_node_t* node)
//{
//   (void)node;
//   return false;
// }

/*!
 * @brief: Try to split a node into two
 *
 * We need @node to have enough room for both data of size @size AND two more heap_node_t structs.
 * This is because we want to keep the ability to split nodes.
 */
static heap_node_t* split_node(heap_node_buffer_t* buffer, heap_node_t* node, size_t size)
{
    size_t node_size;
    heap_node_t* ret;

    /* Can't split used nodes */
    if (heap_node_has_flag(node, MALLOC_NODE_FLAG_USED))
        return nullptr;

    node_size = heap_node_get_size(node);

    /* Check if this node has room for @size + 2 * sizeof(*ret) */
    if (node_size < (size + (sizeof(*ret) * 2)))
        return nullptr;

    ret = (heap_node_t*)((uint64_t)&node->data[0] + node_size - (size + sizeof(*ret)));

    /* This boi is going to overflow the buffer, not good */
    if ((u64)ret + (size + sizeof(*ret)) > (u64)buffer + buffer->m_buffer_size)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    /* Fix the return node */
    ret->next = node->next;
    ret->prev = node;
    heap_node_set_size(ret, size);

    if (ret->next)
        ret->next->prev = ret;

    /* Fix the byproduct node */
    node->next = ret;
    heap_node_set_size(node, node_size - (size + sizeof(*ret)));

    return ret;
}

/*!
 * @brief: Check if two nodes can physically merge
 */
static inline bool heap_nodes_can_merge(heap_node_t* node1, heap_node_t* node2)
{
    if (!node1 || !node2)
        return false;

    /* Only free nodes can merge */
    if (heap_node_has_flag(node1, MALLOC_NODE_FLAG_USED) || heap_node_has_flag(node2, MALLOC_NODE_FLAG_USED))
        return false;

    /* One in front of two */
    if (node1->prev == node2 && node2->next == node1)
        return true;

    /* Two in front of one */
    if (node1->next == node2 && node2->prev == node1)
        return true;

    /* Fuck */
    return false;
}

// we check for mergeability again, just for sanity =/
// I'd love to make this more efficient (and readable), but I'm also too lazy xD
static inline heap_node_t* merge_node_with_next(heap_node_buffer_t* buffer, heap_node_t* ptr)
{
    size_t node_size;
    size_t next_size;
    heap_node_t* next;

    next = ptr->next;

    node_size = heap_node_get_size(ptr);
    next_size = heap_node_get_size(next);

    heap_node_set_size(ptr, node_size + next_size + sizeof(heap_node_t));
    ptr->next = next->next;

    if (ptr->next)
        ptr->next->prev = ptr;

    buffer->m_node_count--;

    return ptr;
}

/*!
 * @brief: Actually merge two nodes inside a buffer
 *
 * @returns the heap node created as a result of the merge
 */
static inline heap_node_t* merge_nodes(heap_node_buffer_t* buffer, heap_node_t* ptr1, heap_node_t* ptr2)
{
    if (!heap_nodes_can_merge(ptr1, ptr2))
        return nullptr;

    /* Check which one to merge with next */
    return merge_node_with_next(buffer, (ptr1->next == ptr2) ? ptr1 : ptr2);
}

/*!
 * @brief: Try to merge a node with it's surounding nodes
 */
static inline heap_node_t* try_merge(heap_node_buffer_t* buffer, heap_node_t* node)
{
    heap_node_t* result;

    /* TODO: loop limit */
    while (true) {
        result = merge_nodes(buffer, node, node->next);

        if (!result)
            break;

        node = result;
    }

    while (true) {
        result = merge_nodes(buffer, node, node->prev);

        if (!result)
            break;

        node = result;
    }

    return result;
}

static inline void allocator_add_free(memory_allocator_t* allocator, size_t size)
{
    // KLOG_DBG("kmalloc: +%lld\n", size);
    allocator->m_free_size += size;
    allocator->m_used_size -= size;
}

static inline void allocator_add_used(memory_allocator_t* allocator, size_t size)
{
    // KLOG_DBG("kmalloc: -%lld\n", size);
    allocator->m_used_size += size;
    allocator->m_free_size -= size;
}

#define MEM_ALLOC_DEFAULT_BUFFERSIZE (4 * Mib)
#define MEM_ALLOC_MIN_BUFFERSIZE (64 * Kib) /* 16 Page minimum */

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
    kerror_t error;

    if (!allocator || !size || !(*size))
        return nullptr;

    if (*size < MEM_ALLOC_MIN_BUFFERSIZE)
        *size = MEM_ALLOC_MIN_BUFFERSIZE;

    total_buffer_size = ALIGN_UP(*size + sizeof(heap_node_buffer_t), SMALL_PAGE_SIZE);

    error = __kmem_alloc_range(
        (void**)&ret,
        allocator->m_parent_dir.m_root,
        nullptr,
        KERNEL_MAP_BASE,
        total_buffer_size,
        NULL,
        KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);

    if (error)
        return nullptr;

    /* Total size of the available data */
    data_size = total_buffer_size - sizeof(heap_node_buffer_t) - sizeof(heap_node_t);

    memset(ret, 0, sizeof(heap_node_buffer_t));

    ret->m_buffer_size = total_buffer_size;
    ret->m_node_count = 1;

    start_node = &ret->m_start_node[0];

    memset(start_node, 0, sizeof(heap_node_t));

    /* The size of the node includes only the size of the data region */
    heap_node_set_size(start_node, data_size);
    start_node->next = nullptr;
    start_node->prev = nullptr;

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

    __kmem_dealloc(allocator->m_parent_dir.m_root, nullptr, (uintptr_t)buffer, buffer->m_buffer_size);
}

static inline bool heap_buffer_contains(heap_node_buffer_t* buffer, uintptr_t addr)
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

static kerror_t heap_buffer_allocate_in(memory_allocator_t* allocator, heap_node_buffer_t* buffer, heap_node_t* node, size_t bytes, void** p_res)
{
    size_t node_size;
    heap_node_t* new_node;

    if (!allocator || !buffer || !bytes || !p_res)
        return -1;

    while (node) {
        if (heap_node_has_flag(node, MALLOC_NODE_FLAG_USED))
            goto cycle;

        node_size = heap_node_get_size(node);

        /*
         * Perfect fit: yoink
         */
        if (node_size == bytes) {
            node->attr |= MALLOC_NODE_FLAG_USED;

            allocator_add_used(allocator, node_size);

            *p_res = node->data;
            return 0;
        }

        /* We need enough bytes to store the data + the heap node struct */
        if (node_size > (bytes + sizeof(heap_node_t))) {
            // yay, our node works =D

            // now split off a node of the correct size
            new_node = split_node(buffer, node, bytes);

            if (!new_node)
                goto cycle;

            // for sanity
            new_node->attr |= MALLOC_NODE_FLAG_USED;

            allocator_add_used(allocator, bytes);

            // TODO: edit global shit
            *p_res = new_node->data;
            return 0;
        }

    cycle:
        node = node->next;
    }

    /* Could not split off a node. We need a new buffer */
    return -1;
}

static kerror_t heap_buffer_allocate(memory_allocator_t* allocator, heap_node_buffer_t* buffer, size_t bytes, void** p_res)
{
    /*
     * Just bruteforce it from the start
     * FIXME: should we start from the end actually?
     */
    return heap_buffer_allocate_in(allocator, buffer, buffer->m_start_node, bytes, p_res);
}

static kerror_t heap_buffer_deallocate(memory_allocator_t* allocator, heap_node_buffer_t* buffer, void* addr)
{
    // first we'll check if we can do this easily
    heap_node_t* node = TO_NODE(addr);

    /* Wrong buffer */
    if (!heap_buffer_contains(buffer, (uintptr_t)node))
        return -1;

    if (!heap_node_has_flag(node, MALLOC_NODE_FLAG_USED))
        return -1;

    allocator_add_free(
        allocator,
        heap_node_get_size(node));

    /* Clear the nodes free flag */
    node->attr &= ~MALLOC_NODE_FLAG_USED;

    /* Mergeeee */
    try_merge(buffer, node);

    // FIXME: should we zero freed nodes?
    return (0);
}

int malloc_heap_init(memory_allocator_t* allocator, void* buffer, size_t size, uint32_t flags)
{
    heap_node_t* initial_node;
    heap_node_buffer_t* initial_buffer;

    if (!allocator || !buffer || !size)
        return -1;

    memset(allocator, 0, sizeof(*allocator));

    /* Initialize the kernel allocator */
    allocator->m_free_size = size;
    allocator->m_flags = flags;
    allocator->m_used_size = 0;

    /* Pretty good dummy for the parent dir lmao */
    allocator->m_parent_dir = (page_dir_t) {
        .m_root = nullptr,
        .m_kernel_low = HIGH_MAP_BASE,
        .m_kernel_high = 0xFFFFFFFFFFFFFFFF,
    };

    /* Initialize the buffer */
    initial_buffer = (heap_node_buffer_t*)buffer;

    memset(buffer, 0, size);

    /* Set up the buffer */
    initial_buffer->m_node_count = 1;
    initial_buffer->m_buffer_size = size;
    initial_buffer->m_next = nullptr;

    /* Set up the initial node */
    initial_node = initial_buffer->m_start_node;

    initial_node->prev = nullptr;
    initial_node->next = nullptr;
    heap_node_set_size(initial_node, size - sizeof(heap_node_buffer_t) - sizeof(*initial_node));

    /* Give the buffer to the allocator */
    allocator->m_buffers = initial_buffer;

    return 0;
}

void malloc_heap_dump(memory_allocator_t* allocator)
{
    heap_node_buffer_t* buffer;

    buffer = allocator->m_buffers;

    KLOG_DBG("malloc_heap_dump: Checking buffer with (%lld) bytes and (%lld) nodes\n", buffer->m_buffer_size, buffer->m_node_count);

    for (heap_node_t* node = buffer->m_start_node; node; node = node->next) {
        KLOG_DBG("(%s) Heap node of size: %lld (0x%llx)\n", heap_node_has_flag(node, MALLOC_NODE_FLAG_USED) ? "used" : "free", heap_node_get_size(node), heap_node_get_size(node));
    }
}

/*!
 * @brief: Create a malloc heap
 */
memory_allocator_t* create_malloc_heap(size_t size, vaddr_t virtual_base, uintptr_t flags)
{
    kernel_panic("TODO: create_malloc_heap");
}

/*!
 * @brief: Destroy a malloc heap
 */
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
void* memory_allocate(memory_allocator_t* allocator, size_t bytes)
{
    kerror_t error;
    void* result;
    heap_node_buffer_t* current_buffer;

    if (!allocator || !bytes)
        return nullptr;

    /* Ensure alignment */
    bytes = ALIGN_UP(bytes, 8);
    current_buffer = allocator->m_buffers;

    /*
     * Loop over all the linked buffers and try to allocate in them
     */
    while (current_buffer) {

        error = heap_buffer_allocate(allocator, current_buffer, bytes, &result);

        if (!error)
            return result;

        current_buffer = current_buffer->m_next;
    }

    /*
     * TODO: intelligently calculate a best new size for this buffer
     * (
     *    Perhaps by taking timestamps in between heap usages and trying to
     *    determine what size is most used?
     * )
     */

    heap_node_buffer_t* new_buffer;
    size_t extra_size = MEM_ALLOC_DEFAULT_BUFFERSIZE;
    size_t total_size = allocator->m_used_size + allocator->m_free_size;

    /* Could not allocate in any existing buffer. Try to create a new one */
    new_buffer = create_heap_node_buffer(allocator, &extra_size);

    KLOG_DBG("Malloc expantion (size: 0x%llx+0x%llx)\n", total_size, extra_size);

    /* FUCKKK */
    if (!new_buffer)
        return nullptr;

    error = heap_buffer_allocate(allocator, new_buffer, bytes, &result);

    if (error)
        return nullptr;

    return result;
}

void memory_sized_deallocate(memory_allocator_t* allocator, void* addr, size_t allocation_size)
{
    kernel_panic("TODO: memory_sized_deallocate");
}

void memory_deallocate(memory_allocator_t* allocator, void* addr)
{
    heap_node_buffer_t* current_buffer;

    if (!addr || !allocator)
        return;

    current_buffer = allocator->m_buffers;

    while (current_buffer) {

        if (!heap_buffer_deallocate(allocator, current_buffer, addr))
            break;

        current_buffer = current_buffer->m_next;
    }

    // println("Could not find buffer to deallocate in!");
}

// just expand the heap by some page-aligned amount
ANIVA_STATUS memory_try_heap_expand(memory_allocator_t* allocator, size_t new_size)
{
    return ANIVA_FAIL;
}
