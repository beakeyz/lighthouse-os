#include "bitmap.h"
#include "libk/flow/error.h"
#include <libk/string.h>
#include <mem/heap.h>

#ifndef BITMAP_DEFAULT
#define BITMAP_DEFAULT 0xff
#endif

void init_bitmap(bitmap_t* bitmap, size_t max_entries, size_t mapsize, uint8_t default_value)
{
    if (!bitmap)
        return;

    bitmap->m_default = default_value;
    bitmap->m_size = mapsize;
    bitmap->m_entries = max_entries;

    memset(bitmap->m_map, default_value, mapsize);
}

// We leave the calculation of the amount of
// bytes for this map to the caller
bitmap_t* create_bitmap(size_t size)
{
    bitmap_t* map;

    if (!size)
        return nullptr;

    map = kmalloc(sizeof(bitmap_t) + size);

    if (!map)
        return nullptr;

    /* Clear it out */
    memset(map, 0, sizeof(bitmap_t));

    /* Init the thing */
    init_bitmap(map, size << 3, size, BITMAP_DEFAULT);

    return map;
}

bitmap_t* create_bitmap_ex(size_t max_entries, uint8_t default_value)
{
    size_t bitmap_size;
    bitmap_t* map;

    bitmap_size = max_entries >> 3;

    if (!bitmap_size)
        return nullptr;

    map = kmalloc(sizeof(*map) + bitmap_size);

    if (!map)
        return nullptr;

    init_bitmap(map, max_entries, bitmap_size, default_value);

    return map;
}

bitmap_t* create_bitmap_with_default(size_t size, uint8_t default_value)
{
    return create_bitmap_ex(size << 3, default_value);
}

void destroy_bitmap(bitmap_t* map)
{
    if (!map)
        return;

    kfree(map->m_map);
    kfree(map);
}

void bitmap_mark(bitmap_t* this, uint32_t index)
{
    bitmap_mark_range(this, index, 1);
}

void bitmap_unmark(bitmap_t* this, uint32_t index)
{
    bitmap_unmark_range(this, index, 1);
}

void bitmap_unmark_range(bitmap_t* this, uint32_t index, size_t length)
{
    if (index >= this->m_entries) {
        // printf("bitmap_unmark_range: index: %d, entries: %lld, length: %lld\n", index, this->m_entries, length);
        return;
        // kernel_panic("Huh");
    }

    for (uint32_t i = 0; i < length; i++)
        this->m_map[(index + i) >> 3] &= ~(1 << ((index + i) & 7));
}

void bitmap_mark_range(bitmap_t* this, uint32_t index, size_t length)
{
    if (index >= this->m_entries) {
        // printf("bitmap_mark_range: index: %d, entries: %lld, length: %lld\n", index, this->m_entries, length);
        return;
    }

    for (uint32_t i = 0; i < length; i++)
        this->m_map[(index + i) >> 3] |= (1 << ((index + i) & 7));
}

int bitmap_find_free_range_ex(bitmap_t* this, size_t length, enum BITMAP_SEARCH_DIR dir, uintptr_t* p_result)
{
    kernel_panic("TODO: bitmap_find_free_range_ex");
}
int bitmap_find_free_range_from_ex(bitmap_t* this, size_t length, uintptr_t start_idx, enum BITMAP_SEARCH_DIR dir, uintptr_t* p_result)
{
    kernel_panic("TODO: bitmap_find_free_range_from_ex");
}

/*!
 * @brief: Find the largest sequence of 0 bits inside a qword
 *
 * Since free entries inside bitmaps are denoted by zero bits, we want to quickly find the largest sequence of
 * 0 bits inside a qword, together with it's offset inside the qword
 *
 * byte: 0b01011000 -> should give: freelen=3, offset=2
 */
static inline u32 __bitmap_qword_get_largest_freelen(u64 _qword, u32* pOffset, bool* pIsEnd, bool* pIsStart)
{
    /* Full qword, skip it */
    if (_qword == 0xffFFffFFffFFffFFULL)
        return 0;

    // TODO:
    return 64;
}

int bitmap_find_free_range(bitmap_t* this, size_t length, uintptr_t* p_result)
{
    u32 c_offset;
    u32 c_freelen;
    u64 _qword;
    bool is_end, is_start;

    /* This function is always going to be faster */
    if (length == 1)
        return bitmap_find_free(this, p_result);

    /* Loop over the map with a qword boundry */
    for (u64 i = 0; i < (this->m_entries >> 6); i++) {
        /* Grab the qword here (this is so hacky) */
        _qword = *(u64*)&this->m_map[i << 3];

        /* Try to grab the freelen */
        c_freelen = __bitmap_qword_get_largest_freelen(_qword, &c_offset, &is_end, &is_start);

        /* This qword had no free bits */
        if (!c_freelen)
            continue;
        /*
         * We can perhaps cache the last found range so the next search
         * can start from that index, until the index gets 'corrupted' by
         * a free or something before the cached index
         */
        return bitmap_find_free_range_from(this, length, i << 3, p_result);
    }

    return -1;
}

// TODO: find out if this bugs out
int bitmap_find_free_range_from(bitmap_t* this, size_t length, uintptr_t start_idx, uintptr_t* p_result)
{
    if (!length || !p_result)
        return -1;

    /* lol, we can never find anything if this happens :clown: */
    if (start_idx + length >= this->m_entries)
        return -1;

    for (uintptr_t i = start_idx; i < this->m_entries; i++) {

        if (bitmap_isset(this, i))
            continue;

        uintptr_t j = 1;

        while (j < length) {

            if (bitmap_isset(this, i + j))
                break;

            j++;
        }

        if (j != length)
            goto cycle;

        *p_result = i;
        return 0;
    cycle:
        i += j;
    }

    return 0;
}

// returns the index of the free bit
int bitmap_find_free(bitmap_t* this, uintptr_t* p_result)
{
    if (!p_result)
        return -1;

    /*
     * Let's hope we keep this in cache ;-;
     */

    for (uintptr_t i = 0; i < this->m_size; i++) {
        if (this->m_map[i] == 0xff)
            continue;

        uintptr_t start_idx = BYTES_TO_BITS(i);

        for (uintptr_t j = 0; j < 8; j++) {
            if (bitmap_isset(this, start_idx + j))
                continue;

            *p_result = start_idx + j;
            return 0;
        }
    }

    return -KERR_NOT_FOUND;
}

bool bitmap_isset(bitmap_t* this, uint32_t index)
{
    if (index >= this->m_entries) {
        return true;
    }

    // ASSERT_MSG(index < this->m_entries, "Bitmap (bitmap_isset): index out of bounds");

    const uint64_t index_byte = index >> 3ULL;
    const uint32_t index_bit = index % 8UL;

    return (this->m_map[index_byte] & (1 << index_bit));
}
