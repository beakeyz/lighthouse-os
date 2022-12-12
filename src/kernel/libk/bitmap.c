#include "bitmap.h"
#include "dev/debug/serial.h"
#include <mem/kmalloc.h>
#include <libk/string.h>

#ifndef BITMAP_DEFAULT
  #define BITMAP_DEFAULT 0xff
#endif

static inline void __init_bitmap_late(bitmap_t* map);

bitmap_t init_bitmap(size_t size) {
  
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
    __init_bitmap_late(&map);
  }
  return map;
}

void bitmap_mark(bitmap_t* this, uint32_t index) {
  if (index > this->m_entries) {
    return;
  }

  const uint32_t index_byte = index / 8;
  const uint32_t index_bit = index % 8;

  this->m_map[index_byte] |= (1 << index_bit);
}

void bitmap_unmark(bitmap_t* this, uint32_t index) {
  if (index > this->m_entries) {
    return;
  }

  const uint32_t index_byte = index / 8;
  const uint32_t index_bit = index % 8;

  this->m_map[index_byte] &= ~(1 << index_bit);
}

// TODO
uintptr_t bitmap_find_free_range(bitmap_t* this, uint32_t index, size_t length) {
  return 0;
}

// returns the index of the free bit
uintptr_t bitmap_find_free(bitmap_t* this) {
  // doodoo linear scan >=(
  // TODO: make not crap
  for (uintptr_t i = 0; i < this->m_entries; i++) {
    if (!this->fIsSet(this, i)) {
      return i;
    }
  }

  return 0;
}

bool bitmap_isset(bitmap_t* this, uint32_t index) {
  if (index > this->m_entries) {
    return false;
  }

  const uint32_t index_byte = index / 8;
  const uint32_t index_bit = index % 8;

  return (this->m_map[index_byte] & (1 << index_bit));
}

static inline void __init_bitmap_late(bitmap_t* map) {
  if (map == nullptr || !map->m_map) {
    return;
  }

  map->fMark = bitmap_mark;
  map->fUnmark = bitmap_unmark;
  map->fIsSet = bitmap_isset;
  map->fFindFree = bitmap_find_free;
  map->fFindFreeRange = bitmap_find_free_range;
}
