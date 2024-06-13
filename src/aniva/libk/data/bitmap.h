#ifndef __ANIVA_BITMAP__
#define __ANIVA_BITMAP__

#include <libk/stddef.h>

struct bitmap;

// 8-bit bitmap
typedef struct bitmap {
    uint8_t m_default;
    size_t m_size; // size in bytes
    size_t m_entries; // size in bits
    uint8_t m_map[];
} bitmap_t;

// #define BITMAP_INVALID_LASTFREE  ((uint32_t)-1)
#define BITS_TO_BYTES(bits) ((bits) >> 3)
#define BYTES_TO_BITS(bytes) ((bytes) << 3)

enum BITMAP_SEARCH_DIR {
    FORWARDS = 0,
    BACKWARDS,
};

bitmap_t* create_bitmap(size_t size);
bitmap_t* create_bitmap_with_default(size_t size, uint8_t default_value);
bitmap_t* create_bitmap_ex(size_t max_entries, uint8_t default_value);
void init_bitmap(bitmap_t* bitmap, size_t max_entries, uint8_t default_value); // TODO:
void destroy_bitmap(bitmap_t* map);

void bitmap_mark(bitmap_t* this, uint32_t index);
void bitmap_unmark(bitmap_t* this, uint32_t index);

bool bitmap_isset(bitmap_t* this, uint32_t index);

int bitmap_find_free(bitmap_t* this, uintptr_t* p_result);
int bitmap_find_free_range(bitmap_t* this, size_t length, uintptr_t* p_result);
int bitmap_find_free_range_from(bitmap_t* this, size_t length, uintptr_t start_idx, uintptr_t* p_result);

int bitmap_find_free_ex(bitmap_t* this, enum BITMAP_SEARCH_DIR dir, uintptr_t* p_result);
int bitmap_find_free_range_ex(bitmap_t* this, size_t length, enum BITMAP_SEARCH_DIR dir, uintptr_t* p_result);
int bitmap_find_free_range_from_ex(bitmap_t* this, size_t length, uintptr_t start_idx, enum BITMAP_SEARCH_DIR dir, uintptr_t* p_result);

void bitmap_mark_range(bitmap_t* this, uint32_t index, size_t length);
void bitmap_unmark_range(bitmap_t* this, uint32_t index, size_t length);

#endif // !
