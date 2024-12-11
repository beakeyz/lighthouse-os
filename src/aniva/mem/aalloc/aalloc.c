#include "aalloc.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include <lightos/error.h>
#include <mem/kmem_manager.h>
#include <stdlib.h>

/*!
 * @brief: Initialize arena memory
 *
 * A single arena will keep track of a buffer and an offset into that buffer
 * that tells us how 'full' the arena is
 */
static inline int __init_arena(arena_t* arena, void* buffer, size_t sz)
{
    if (!arena)
        return -EINVAL;

    arena->size = sz;
    arena->start = buffer;
    arena->offset = buffer;
    arena->next = nullptr;
    return 0;
}

/*!
 * @brief: Allocate an arena from thin air
 */
static inline arena_t* __allocate_arena(size_t sz)
{
    arena_t* ret;
    void* buffer;

    if (!sz)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    sz = ALIGN_UP(sz, SMALL_PAGE_SIZE);

    if (!__kmem_kernel_alloc_range(&buffer, sz, NULL, KMEM_FLAG_WRITABLE)) {
        kfree(ret);
        return nullptr;
    }

    return ret;
}

static inline void __deallocate_arena(arena_t* a)
{
    ASSERT(a->start && a->size);

    /* Deallocate this memory */
    __kmem_kernel_dealloc((vaddr_t)a->start, a->size);

    /* Free the arena */
    kfree(a);
}

static inline int __arena_clear(arena_t* arena)
{
    if (!arena)
        return -EINVAL;

    arena->offset = arena->start;
    return 0;
}

static inline void* __arena_allocate(arena_t* arena, size_t sz)
{
    void* ret;

    if (!arena || !sz)
        return nullptr;

    /* Check for an overflow */
    if ((arena->offset + sz) > (arena->start + arena->size))
        return nullptr;

    /* Add shit to the offset */
    ret = arena->offset;
    arena->offset += sz;

    return ret;
}

/*!
 * @brief: Initialize an arena allocator
 *
 * NOTE: @max_arena_nr = 0 implies a maximum amount of arenas, @max_arena_nr = 0 implies a maximum arena size
 */
int init_a_allocator(a_allocator_t* allocator, u32 flags, u32 max_arena_nr, u32 max_arena_sz, u32 fixed_allocation_sz, void* buffer, size_t bsize)
{
    int error;

    if (!allocator)
        return -EINVAL;

    memset(allocator, 0, sizeof(*allocator));

    /* Put in the flags */
    allocator->flags = flags;
    /* We start out with one arena */
    allocator->arena_nr = 1;
    /* Set the limit sizes */
    allocator->max_arena_sz = (max_arena_sz == 0) ? 0 : ALIGN_UP(max_arena_sz, SMALL_PAGE_SIZE);
    allocator->max_arena_nr = max_arena_nr;

    /* Check if we're a fixed-size allocator */
    if ((flags & AALLOC_FLAG_FIXED_SZ) == AALLOC_FLAG_FIXED_SZ)
        allocator->fixed_allocation_sz = fixed_allocation_sz;

    /* If our kind soul provided a buffer for us, we're happy =) */
    if (!buffer || !bsize) {
        /* NOOO now we have to allocate shit ourselves =( */
        error = __kmem_kernel_alloc_range(&buffer, AALLOC_DEFAULT_ARENA_SZ, NULL, KMEM_FLAG_WRITABLE);

        /* Fuck */
        if (error)
            return error;

        /* Update the buffersize variable */
        bsize = AALLOC_DEFAULT_ARENA_SZ;
        allocator->flags |= AALLOC_FLAG_KMEM_START;
    }

    /* Initialize the start arena */
    __init_arena(&allocator->start_arena, buffer, bsize);

    return 0;
}

int init_basic_aalloc(a_allocator_t* alloc, void* buffer, size_t bsize)
{
    /* Initializes a basic arena allocator, of default size, with one starting arena */
    return init_a_allocator(alloc, NULL, NULL, NULL, NULL, buffer, bsize);
}

a_allocator_t* create_a_allocator(u32 flags, u32 max_arena_nr, u32 max_arena_sz, u32 fixed_allocation_sz, void* buffer, size_t bsize)
{
    a_allocator_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    /* NOTE: If we succeed to initialize the allocator, we return the allocator */
    if (!init_a_allocator(ret, flags, max_arena_nr, max_arena_sz, fixed_allocation_sz, buffer, bsize))
        return ret;

    kfree(ret);
    return nullptr;
}

void destroy_a_allocator(a_allocator_t* allocator)
{
    /* Start arena was allocated by us, we need to delete it  */
    if ((allocator->flags & AALLOC_FLAG_KMEM_START) != AALLOC_FLAG_KMEM_START)
        __kmem_kernel_dealloc((vaddr_t)allocator->start_arena.start, allocator->start_arena.size);

    /*
     * For now, other arenas will be put on the kernel heap
     * FIXME: Find a nicer way to store the arena structures themselves
     */
    for (arena_t* a = allocator->start_arena.next; a != nullptr; a = a->next)
        __deallocate_arena(a);

    kfree(allocator);
}

/*
 * Utility functions
 */

void aalloc_clear(a_allocator_t* alloc)
{
    for (arena_t* a = alloc->start_arena.next; a != nullptr; a = a->next) {
        if ((alloc->flags & AALLOC_FLAG_CLEAR_CHAIN) == AALLOC_FLAG_CLEAR_CHAIN)
            __arena_clear(a);
        else
            __deallocate_arena(a);
    }

    /* Clear the start arena */
    __arena_clear(&alloc->start_arena);

    /* Also ensure the start arena forgets it's chain */
    if ((alloc->flags & AALLOC_FLAG_CLEAR_CHAIN) != AALLOC_FLAG_CLEAR_CHAIN)
        alloc->start_arena.next = nullptr;
}

void* aalloc_allocate(a_allocator_t* alloc, size_t sz)
{
    void* ret;
    arena_t** slot;
    arena_t* start = &alloc->start_arena;

    for (slot = &start; *slot; slot = &(*slot)->next) {
        /* Try and allocate in the allocator */
        ret = __arena_allocate(*slot, sz);

        /* Allocation succeeded, return */
        if (ret)
            return ret;
    }

    /* Limit exceeded. Bail */
    if (alloc->arena_nr >= alloc->max_arena_nr)
        return nullptr;

    /* Try to append a new arena to the chain */
    *slot = __allocate_arena((alloc->max_arena_sz == 0) ? AALLOC_DEFAULT_ARENA_SZ : alloc->max_arena_sz);

    /* Also increment the arena count */
    alloc->arena_nr++;

    /* Let's hope this allocation doen't fail :clown: */
    return __arena_allocate(*slot, sz);
}

/*!
 * @brief: Function that calculates the used size and free size of the allocator
 */
int aalloc_get_info(a_allocator_t* alloc, size_t* p_used_sz, size_t* p_free_sz)
{
    size_t used_sz = 0;
    size_t free_sz = 0;

    if (!alloc || !p_used_sz || !p_free_sz)
        return -EBADPARAMS;

    for (arena_t* a = &alloc->start_arena; a; a = a->next) {
        /* How far is the offset pointer into the buffer */
        used_sz += (a->offset - a->start);
        /* How much space is there left in this arena */
        free_sz += a->size - (a->offset - a->start);
    }

    /* Export the calculated values */
    *p_used_sz = used_sz;
    *p_free_sz = free_sz;

    return 0;
}
