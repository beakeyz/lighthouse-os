#include "memory.h"
#include "lightos/memory/alloc.h"
#include "lightos/memory/memflags.h"
#include <assert.h>
#include <lightos/system.h>
#include <stdio.h>
#include <string.h>

/*
 * TODO: I hate this impl: fix it
 */

#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

#define POOLENTRY_USED_BIT 0x8000000000000000ULL
/*
 * A single entry inside the pool
 */
struct malloc_pool_entry {
    uintptr_t attr;

    uint8_t data[];
};

static inline void* _poolentry_get_data(struct malloc_pool_entry* entry)
{
    return entry->data;
}

static inline struct malloc_pool_entry* _data_get_malloc_pool_entry(void* data)
{
    return (struct malloc_pool_entry*)((uintptr_t)data - sizeof(struct malloc_pool_entry));
}

static inline uintptr_t _poolentry_get_next_free(struct malloc_pool_entry* entry)
{
    return (uintptr_t)(entry->attr & ~(POOLENTRY_USED_BIT));
}

static inline void _poolentry_set_next_free(struct malloc_pool_entry* entry, uintptr_t next)
{
    /* This should really never happen xD */
    assert((next & POOLENTRY_USED_BIT) != POOLENTRY_USED_BIT);

    entry->attr &= POOLENTRY_USED_BIT;
    entry->attr |= next;
}

/*
 * A pool of dynamic memory
 *
 * Pools are organised as followed: We order pools based on their entry_size field. We group pools together with
 * the same entry_sizes in a linked list.
 */
struct malloc_pool {
    struct malloc_pool* m_next_size;
    struct malloc_pool* m_next_sibling;
    size_t m_data_size;
    size_t m_entry_size;
    uint8_t m_poolentries[];
};

static inline size_t _get_real_entrysize(size_t entrysize)
{
    return entrysize + sizeof(struct malloc_pool_entry);
}

static inline struct malloc_pool_entry* _pool_get_entry(struct malloc_pool* pool, uintptr_t idx)
{
    idx *= _get_real_entrysize(pool->m_entry_size);

    if (idx >= pool->m_data_size)
        return nullptr;

    return (struct malloc_pool_entry*)&pool->m_poolentries[idx];
}

static inline bool _pool_is_valid_pointer(struct malloc_pool* pool, void* data, struct malloc_pool_entry** bentry)
{
    struct malloc_pool_entry* entry;

    if (!data)
        return false;

    /* Data lays outside of the pools valid range */
    if (data < (void*)pool->m_poolentries || data >= (void*)(pool->m_poolentries + pool->m_data_size))
        return false;

    entry = _data_get_malloc_pool_entry(data);

    /* TODO: Verify the entries hash */
    if (bentry)
        *bentry = entry;
    return true;
}

static struct malloc_pool* __start_range;

/*!
 * @brief: Find a pool which supports our allocationsize
 *
 * This only finds the top size pool. This should be the head of the sibling chain
 */
static struct malloc_pool* _find_malloc_pool(size_t entrysize)
{
    struct malloc_pool* walker;

    walker = __start_range;

    while (walker) {
        if (walker->m_entry_size == ALIGN_UP(entrysize, MEMPOOL_ALIGN))
            return walker;

        walker = walker->m_next_size;
    }

    return nullptr;
}

/*!
 * @brief: Calculate what the max entrycount is going to be for a given entrysize
 *
 * 'calculate'
 */
static inline size_t _calculate_poolsize(size_t entrysize)
{
    size_t coefficient;

    /*
     * AHHHHH MY EYESOCKETS
     */
    if (entrysize <= (16 * Kib))
        coefficient = 64;
    else if (entrysize <= (64 * Kib))
        coefficient = 16;
    else
        coefficient = 1;

    return _get_real_entrysize(entrysize) * coefficient;
}

static struct malloc_pool* _create_malloc_pool(size_t entrysize)
{
    size_t poolsize;
    struct malloc_pool* ret;

    entrysize = ALIGN_UP(entrysize, MEMPOOL_ENTSZ_ALIGN);

    poolsize = _calculate_poolsize(entrysize) + sizeof(*ret);

    /* Allocate a memorypool where we can put our malloc pool */
    ret = allocate_vmem(poolsize, VMEM_FLAG_READ | VMEM_FLAG_WRITE);

    /* Syscall memory for us */
    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->m_entry_size = entrysize;
    ret->m_data_size = ALIGN_DOWN(poolsize - sizeof(*ret), _get_real_entrysize(ret->m_entry_size));
    ret->m_next_sibling = NULL;
    ret->m_next_size = NULL;

    memset(ret->m_poolentries, 0, ret->m_data_size);

    return ret;
}

static inline int _pool_append_size(struct malloc_pool* pool, struct malloc_pool* to_append)
{
    /* Inherit the next size */
    to_append->m_next_size = pool->m_next_size;
    to_append->m_next_sibling = NULL;

    /* Walk the entire sibling chain to update the links */
    while (pool) {
        pool->m_next_size = to_append;

        /* Cycle through the siblings */
        pool = pool->m_next_sibling;
    }

    return 0;
}

static inline int _pool_append_sibling(struct malloc_pool* pool, struct malloc_pool* to_append)
{
    struct malloc_pool** slot;

    /* Inherit the next size */
    to_append->m_next_size = pool->m_next_size;
    to_append->m_next_sibling = NULL;

    slot = &pool;

    while (*slot)
        slot = &(*slot)->m_next_sibling;

    /* Finally place the pool into it's slot */
    *slot = to_append;

    return 0;
}

static int _register_malloc_pool(struct malloc_pool* pool)
{
    struct malloc_pool* walker;

    walker = __start_range;

    do {
        if (walker->m_entry_size == pool->m_entry_size)
            return _pool_append_sibling(walker, pool);

        if (pool->m_entry_size > walker->m_entry_size)
            /*
             * If we either reached the end of the chain, meaning the pool we're adding is the largest pool yet,
             * or if this pools entrysize is in between that of the walkers entrysize and it's next entrysize, we
             * can append it
             */
            if (!walker->m_next_size || walker->m_next_size->m_entry_size > pool->m_entry_size)
                return _pool_append_size(walker, pool);

        walker = walker->m_next_size;
    } while (walker);

    /* Could not append for some reason */
    return -1;
}

static void* _try_pool_allocate(struct malloc_pool* pool)
{
    size_t entrysize;
    size_t entrycount;
    struct malloc_pool_entry* entry;

    entrysize = _get_real_entrysize(pool->m_entry_size);
    entrycount = pool->m_data_size / entrysize;

    /*
     * Simple linear scan to get this fucking shit done right now lmao
     */
    for (uint64_t i = 0; i < entrycount; i++) {
        entry = (struct malloc_pool_entry*)&pool->m_poolentries[i * entrysize];

        if ((entry->attr & POOLENTRY_USED_BIT) == POOLENTRY_USED_BIT)
            continue;

        entry->attr |= POOLENTRY_USED_BIT;

        memset(entry->data, 0, pool->m_entry_size);

        return _poolentry_get_data(entry);
    }

    return nullptr;
}

/*!
 * @brief: Try to yoink a pool entry
 *
 * TODO: optimize
 */
static void* _pool_allocate(struct malloc_pool* pool)
{
    void* ret;
    struct malloc_pool* extra_pool;

    /* Try to allocate in every pool that we have */
    do {
        ret = _try_pool_allocate(pool);

        if (ret)
            return ret;

        pool = pool->m_next_sibling;
    } while (pool);

    /* Create and extra pool */
    extra_pool = _create_malloc_pool(pool->m_entry_size);

    /* Try to see if we can allocate inside of a new pool =/ */
    _pool_append_sibling(
        pool,
        extra_pool);

    /* Try to allocate here */
    return _try_pool_allocate(extra_pool);
}

/*!
 * @brief Internal memory allocation routine
 *
 * Based on the size, we'll see if we can find a pool to allocate in. If we can't find
 * anything, we'll simply create a new one
 */
void* mem_alloc(size_t size)
{
    struct malloc_pool* pool;

    /* Make sure the size is aligned right */
    size = ALIGN_UP(size, MEMPOOL_ENTSZ_ALIGN);

    pool = _find_malloc_pool(size);

    if (!pool) {
        pool = _create_malloc_pool(size);

        /* No memory left I guess? */
        if (!pool)
            return nullptr;

        /* This is very stoopid */
        if (_register_malloc_pool(pool) != 0)
            return nullptr;
    }

    return _pool_allocate(pool);
}

void* mem_move_alloc(void* addr, size_t new_size)
{
    return nullptr;
}

/*!
 * @brief: Find the pool and entry that correspond to a given address @addr
 */
static int _resolve_pool_entry(void* addr, struct malloc_pool** bpool, struct malloc_pool_entry** bentry)
{
    struct malloc_pool* walker;
    struct malloc_pool* sibling_walker;

    /* Null is always invalid */
    if (!addr)
        return -1;

    sibling_walker = nullptr;
    walker = __start_range;

    /* Walk the top-level pool chain */
    while (walker) {
        sibling_walker = walker;

        /* Walk the sibling chain */
        do {

            if (_pool_is_valid_pointer(sibling_walker, addr, bentry))
                break;

            sibling_walker = sibling_walker->m_next_sibling;
        } while (sibling_walker);

        walker = walker->m_next_size;
    }

    /* Could not find a valid pool */
    if (!sibling_walker)
        return -1;

    /* Set the pool buffer. _pool_is_valid_pointer already populates @bentry */
    if (bpool)
        *bpool = sibling_walker;

    return 0;
}

int mem_dealloc(void* addr)
{
    struct malloc_pool* pool;
    struct malloc_pool_entry* entry;

    if (_resolve_pool_entry(addr, &pool, &entry))
        return -1;

    entry->attr &= ~POOLENTRY_USED_BIT;
    return 0;
}

/*
 * Until we have dynamic loading of libraries,
 * we want to keep libraries using libraries to a
 * minimum. This is because when we statically link
 * everything together and for example we are using
 * lightos (Old libsys) here, it gets included in the binary, which itself
 * does not use any other functionallity of lightos (Old libsys), so thats
 * kinda lame imo
 *
 * TODO: check if the process has requested a heap of a specific size
 */
void __init_memalloc(void)
{
    /* Initialize structures */
    /* Ask for a memory region from the kernel */
    /* Use the initial memory we get to initialize the allocator further */
    /* Mark readiness */
    __start_range = _create_malloc_pool(1);
}
