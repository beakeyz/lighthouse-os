#ifndef __KMEM_BITMAP__
#define __KMEM_BITMAP__
#include <libc/stddef.h>

#define MAX_FIND_ATTEMPTS 3

typedef struct kmem_bitmap {
    size_t bm_size;
    uint8_t* bm_buffer;
    size_t bm_last_allocated_bit;
} kmem_bitmap_t;

kmem_bitmap_t make_bitmap ();
kmem_bitmap_t make_data_bitmap (uint8_t* data, size_t len);

// NOTE: pointer or reference?
void bm_set(kmem_bitmap_t* map, size_t idx, bool value);
bool bm_get(kmem_bitmap_t* map, size_t idx);

size_t bm_find (kmem_bitmap_t* map, size_t len);
size_t bm_allocate (kmem_bitmap_t* map, size_t len);
size_t bm_free (kmem_bitmap_t* map, size_t idx, size_t len);

size_t bm_mark_block_used (kmem_bitmap_t* map, size_t idx, size_t len);
size_t bm_mark_block_free (kmem_bitmap_t* map, size_t idx, size_t len);

bool bm_is_in_range (kmem_bitmap_t* map, size_t idx);

#endif // !__KMEM_BITMAP__

