#include "bitmap.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include <mem/heap.h>
#include <libk/string.h>

#ifndef BITMAP_DEFAULT
  #define BITMAP_DEFAULT 0xff
#endif

bitmap_t create_bitmap(size_t size) {
  
  bitmap_t map = {
    .m_default = (uint8_t)BITMAP_DEFAULT,
    .m_size = size,
    .m_entries = size << 3,
    .m_map = nullptr
  };

  if (size > 0) {
    map.m_map = kmalloc(size);
  }

  if (map.m_map) {
    memset(map.m_map, map.m_default, map.m_size);
  }
  return map;
}

bitmap_t create_bitmap_with_default(size_t size, uint8_t default_value) {

  bitmap_t map = {
    .m_default = default_value,
    .m_size = size,
    .m_entries = size << 3,
    .m_map = nullptr
  };

  if (size > 0) {
    map.m_map = kmalloc(size);
  }

  if (map.m_map) {
    memset(map.m_map, map.m_default, map.m_size);
  }
  return map;
}

void destroy_bitmap(bitmap_t *map) {
  kfree(map->m_map);
  kfree(map);
}

// FIXME: ANIVA_STATUS
void bitmap_mark(bitmap_t* this, uint32_t index) {
  if (index > this->m_entries) {
    return;
  }

  const uint32_t index_byte = index / 8;
  const uint32_t index_bit = index % 8;

  this->m_map[index_byte] |= (1 << index_bit);
}

// FIXME: ANIVA_STATUS
void bitmap_unmark(bitmap_t* this, uint32_t index) {
  if (index > this->m_entries) {
    return;
  }

  const uint32_t index_byte = index / 8;
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

// TODO: find out if this bugs out
ErrorOrPtr bitmap_find_free_range(bitmap_t* this, size_t length) {
  if (length >= this->m_entries || length == 0)
    return Error();

  for (uintptr_t i = 0; i < this->m_entries; i++) {
    uintptr_t length_check = 0;

    if (!bitmap_isset(this, i)) {
      // lil double check
      for (uintptr_t j = i; j < i+length; j++) {
        if (bitmap_isset(this, j)) {
          break;
        }
        length_check++;
      }
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

  const uint32_t index_byte = index / 8;
  const uint32_t index_bit = index % 8;

  return (this->m_map[index_byte] & (1 << index_bit));
}
