#ifndef __ANIVA_MEM_AALLOC__
#define __ANIVA_MEM_AALLOC__

#include <libk/stddef.h>

/*
 * Next level allocator: The arena allocator
 *
 * Meant to provide a cache-friendly, simple and widely useable allocation
 * solution, without much overhead. The principle of an arena allocator is very
 * simple. We simply have a big buffer with a pointer that keeps track of the
 * last allocation. Every allocation moves the pointer forward and all allocations
 * are 'final'. This means you can't free memory allocations with this allocator =(.
 */

typedef struct arena {
    /* The size of this arena */
    size_t size;
    /* Start of this arena buffer */
    void* start;
    /* Allocation offset for this arena */
    void* offset;
    /* Next arena in a possible arena-chain */
    struct arena* next;
} arena_t;

#define AALLOC_DEFAULT_ARENA_SZ (16 * Kib)

/* We can do fixed-sized allocations with this allocator */
#define AALLOC_FLAG_FIXED_SZ 0x00000001UL
/* Start arena lives in a kmem area */
#define AALLOC_FLAG_KMEM_START 0x00000002UL
/* With this flag set, the allocator will clear all chained arenas, in stead of deallocating them */
#define AALLOC_FLAG_CLEAR_CHAIN 0x00000004UL

typedef struct a_allocator {
    u32 flags;
    u32 max_arena_sz;
    u16 max_arena_nr;
    u16 arena_nr;
    union {
        /* How big are allocation, in the case of fixed allocations */
        u32 fixed_allocation_sz;
        /* If allocations aren't fixed, we have this space reserved =/ */
    };

    /* The first arena of this allocator */
    arena_t start_arena;
} a_allocator_t;

int init_a_allocator(a_allocator_t* allocator, u32 flags, u32 max_arena_nr, u32 max_arena_sz, u32 fixed_allocation_sz, void* buffer, size_t bsize);
int init_basic_aalloc(a_allocator_t* alloc, void* buffer, size_t bsize);
a_allocator_t* create_a_allocator(u32 flags, u32 max_arena_nr, u32 max_arena_sz, u32 fixed_allocation_sz, void* buffer, size_t bsize);
void destroy_a_allocator(a_allocator_t* allocator);

/*
 * Utility functions
 */

void aalloc_clear(a_allocator_t* alloc);
void* aalloc_allocate(a_allocator_t* alloc, size_t sz);
void* aalloc_allocate_fixed(a_allocator_t* alloc);
int aalloc_get_info(a_allocator_t* alloc, size_t* p_used_sz, size_t* p_free_sz);

#endif // !__ANIVA_MEM_AALLOC__
