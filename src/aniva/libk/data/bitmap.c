#include "bitmap.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include <mem/heap.h>
#include <libk/string.h>

#ifndef BITMAP_DEFAULT
  #define BITMAP_DEFAULT 0xff
#endif

// We leave the calculation of the amount of 
// bytes for this map to the caller
bitmap_t* create_bitmap(size_t size) {
  
  bitmap_t* map = kmalloc(sizeof(bitmap_t) + size);
  map->m_default = (uint8_t)BITMAP_DEFAULT;
  map->m_size = size;
  map->m_entries = size * 8;

  memset(map->m_map, map->m_default, size);

  return map;
}

bitmap_t* create_bitmap_with_default(size_t size, uint8_t default_value) {

  bitmap_t* map = kmalloc(sizeof(bitmap_t) + size);
  map->m_default = default_value;
  map->m_size = size;
  map->m_entries = size << 3;

  memset(map->m_map, map->m_default, size);

  return map;
}

void destroy_bitmap(bitmap_t *map) {
  if (!map)
    return;

  kfree(map->m_map);
  kfree(map);
}

void bitmap_mark(bitmap_t* this, uint32_t index) {
  if (index >= this->m_entries) {
    return;
  }

  const uint32_t index_byte = index / 8;
  const uint32_t index_bit = index % 8;

  this->m_map[index_byte] |= (1 << index_bit);
}

void bitmap_unmark(bitmap_t* this, uint32_t index) {
  if (index >= this->m_entries) {
    return;
  }

  const uint32_t index_byte = index / 8;
  const uint32_t index_bit = index % 8;

  this->m_map[index_byte] &= ~(1 << index_bit);
}

void bitmap_mark_range(bitmap_t* this, uint32_t index, size_t length) {
  for (uintptr_t i = index; i < index + length; i++) {
    bitmap_mark(this, i);
  }
}

void bitmap_unmark_range(bitmap_t* this, uint32_t index, size_t length) {
  for (uintptr_t i = index; i < index + length; i++) {
    bitmap_unmark(this, i);
  }
}

ErrorOrPtr bitmap_find_free_range(bitmap_t* this, size_t length) {
  /*
   * We can perhaps cache the last found range so the next search 
   * can start from that index, until the index gets 'corrupted' by 
   * a free or something before the cached index
   */
  return bitmap_find_free_range_from(this, length, 0);
}

// TODO: find out if this bugs out
ErrorOrPtr bitmap_find_free_range_from(bitmap_t* this, size_t length, uintptr_t start_idx) {
  if (!length)
    return Error();

  /* lol, we can never find anything if this happens :clown: */
  if (start_idx + length >= this->m_entries)
    return Error();

  for (uintptr_t i = start_idx; i < this->m_entries; i++) {

    if (bitmap_isset(this, i))
      continue;

    uintptr_t j = 1;

    while (j < length) {

      if (bitmap_isset(this, i + j))
        break;

      j++;
    }

    if (j == length)
      return Success(i);

    i += j;
  }

  return Error();
}

// returns the index of the free bit
ErrorOrPtr bitmap_find_free(bitmap_t* this) {

  /*
   * Let's hope we keep this in cache ;-;
   */
  for (uintptr_t i = 0; i < this->m_size; i++) {
    if (this->m_map[i] == 0xff)
      continue;

    uintptr_t start_idx = BYTES_TO_BITS(i);

    for (uintptr_t j = 0; j < 8; j++) {
      if (!bitmap_isset(this, start_idx + j))
        return Success(start_idx + j);
    }
  }

  return Error();
}

bool bitmap_isset(bitmap_t* this, uint32_t index) {

  if (index >= this->m_entries) {
    return true;
  }

  //ASSERT_MSG(index < this->m_entries, "Bitmap (bitmap_isset): index out of bounds");

  const uint64_t index_byte = index >> 3ULL;
  const uint32_t index_bit = index % 8UL;


  return (this->m_map[index_byte] & (1 << index_bit));
}
