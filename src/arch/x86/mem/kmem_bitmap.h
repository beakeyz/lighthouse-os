#ifndef __KMEM_BITMAP__
#define __KMEM_BITMAP__
#include <libc/stddef.h>
#include <arch/x86/multiboot.h>

#define MAX_FIND_ATTEMPTS 3
#define BITMAP_ENTRY_FULL 0xfffffffffffffff
#define BITMAP_ROW_BITS 64

typedef struct kmem_bitmap {
    size_t bm_size;
    uint32_t bm_used_frames;
    uint32_t bm_entry_num;
    size_t bm_last_allocated_bit;
    uint8_t* bm_memory_map;
} kmem_bitmap_t;

void init_bitmap (kmem_bitmap_t* map, struct multiboot_tag_basic_meminfo* basic_info, uint32_t addr);

// NOTE: pointer or reference?
//void bm_set(kmem_bitmap_t* map, size_t idx, bool value);
//bool bm_get(kmem_bitmap_t* map, size_t idx);

size_t bm_find (kmem_bitmap_t* map, size_t len);
size_t bm_allocate (kmem_bitmap_t* map, size_t len);
size_t bm_free (kmem_bitmap_t* map, size_t idx, size_t len);

size_t bm_mark_block_used (kmem_bitmap_t* map, size_t idx, size_t len);
size_t bm_mark_block_free (kmem_bitmap_t* map, size_t idx, size_t len);

bool bm_is_in_range (kmem_bitmap_t* map, size_t idx);
uint32_t find_kernel_entries(uint64_t addr);

void bm_get_region(kmem_bitmap_t* map, uint64_t* base_address, size_t* length_in_bytes);
uint64_t bm_get_frame ();

#endif // !__KMEM_BITMAP__
