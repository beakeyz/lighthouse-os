#include "memory.h"
#include "lightos/memory/alloc.h"
#include "lightos/memory/memflags.h"
#include <assert.h>
#include <lightos/system.h>
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

static inline size_t _get_total_entrysize(size_t entrysize)
{
  return entrysize + sizeof(struct malloc_pool_entry);
}

static inline struct malloc_pool_entry* _pool_get_entry(struct malloc_pool* pool, uintptr_t idx)
{
  idx *= _get_total_entrysize(pool->m_entry_size);

  if (idx >= pool->m_data_size)
    return nullptr;

  return (struct malloc_pool_entry*)&pool->m_poolentries[idx];
}

static inline bool _pool_is_valid_pointer(struct malloc_pool* pool, void* data)
{
  struct malloc_pool_entry* entry;

  if (!data)
    return false;

  entry = _data_get_malloc_pool_entry(data);

  /* TODO: Verify the entries hash */
  (void)entry;
  return false;
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
    if (walker->m_entry_size == entrysize)
      return walker;

    walker = walker->m_next_size;
  }

  return nullptr;
}

static struct malloc_pool* _create_malloc_pool(size_t entrysize)
{
  size_t poolsize;
  struct malloc_pool* ret;

  poolsize = entrysize * 16;

  /* Allocate a memorypool where we can put our malloc pool */
  ret = allocate_pool(&poolsize, MEMPOOL_FLAG_RW, MEMPOOL_TYPE_DEFAULT);

  /* Syscall memory for us */
  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->m_entry_size = entrysize;
  ret->m_data_size = ALIGN_DOWN(poolsize - sizeof(*ret), _get_total_entrysize(ret->m_entry_size));
  ret->m_next_sibling = NULL;
  ret->m_next_size = NULL;

  memset(ret->m_poolentries, 0, ret->m_data_size);

  return ret;
}

static inline int _pool_append_size(struct malloc_pool* pool, struct malloc_pool* to_append) 
{
  /* Inherit the next size */
  to_append->m_next_size = pool->m_next_size;

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

  if (!__start_range) {
    __start_range = pool;
    return 0;
  }

  walker = __start_range;

  do {
    if (pool->m_entry_size > walker->m_entry_size)
      /*
       * If we either reached the end of the chain, meaning the pool we're adding is the largest pool yet,
       * or if this pools entrysize is in between that of the walkers entrysize and it's next entrysize, we
       * can append it
       */
      if (!walker->m_next_size || walker->m_next_size->m_entry_size > pool->m_entry_size)
        return _pool_append_size(walker, pool);

    if (walker->m_entry_size == pool->m_entry_size)
      return _pool_append_sibling(walker, pool);

    walker = walker->m_next_size;
  } while (walker);

  /* Could not append for some reason */
  return -1;
}

/*!
 * @brief: Try to yoink a pool entry
 */
static void* _pool_allocate(struct malloc_pool* pool)
{
  /* TODO: */
  return nullptr;
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

int mem_dealloc(void* addr)
{
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
  __start_range = nullptr;
}
