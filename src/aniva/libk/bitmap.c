#include "bitmap.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
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
  map->m_entries = size << 3;

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

// FIXME: ANIVA_STATUS
void bitmap_mark(bitmap_t* this, uint32_t index) {
  if (index > this->m_entries) {
    return;
  }

  const uint32_t index_byte = index >> 3;
  const uint32_t index_bit = index % 8;

  this->m_map[index_byte] |= (1 << index_bit);
}

// FIXME: ANIVA_STATUS
void bitmap_unmark(bitmap_t* this, uint32_t index) {
  if (index > this->m_entries) {
    return;
  }

  const uint32_t index_byte = index >> 3;
  const uint32_t index_bit = index % 8;

  this->m_map[index_byte] &= ~(1 << index_bit);
}

// FIXME: ANIVA_STATUS
void bitmap_mark_range(bitmap_t* this, uint32_t index, size_t length) {
  for (uintptr_t i = index; i < index + length; i++) {
    bitmap_mark(this, i);
  }
}

// FIXME: ANIVA_STATUS
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
  if (length >= this->m_entries || length == 0)
    return Error();

  /* lol, we can never find anything if this happens :clown: */
  if (start_idx + length > this->m_entries)
    return Error();

  if (length == 1)
    return bitmap_find_free(this);

  for (uintptr_t i = start_idx; i < this->m_entries; i++) {
    uintptr_t length_check = 1;

    if (bitmap_isset(this, i))
      continue;

    // lil double check
    for (uintptr_t j = 1; j < length; j++) {
      if (bitmap_isset(this, i + j))
        break;

      length_check++;
    }

    if (length_check == length) {
      return Success(i);
    }
    i += length_check;
  }

  return Error();
}

// returns the index of the free bit
ErrorOrPtr bitmap_find_free(bitmap_t* this) {
  // doodoo linear scan >=(
  // TODO: make not crap
  for (uintptr_t i = 0; i < this->m_entries; i++) {
    if (!bitmap_isset(this, i)) {
      return Success(i);
    }
  }

  return Error();
}

bool bitmap_isset(bitmap_t* this, uint32_t index) {
  if (index > this->m_entries) {
    return false;
  }

  const uint64_t index_byte = index >> 3ULL;
  const uint32_t index_bit = index % 8UL;

  return (this->m_map[index_byte] & (1 << index_bit));
}
