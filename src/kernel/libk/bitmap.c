#include "bitmap.h"
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

// TODO
void bitmap_mark(uint32_t index) {

}

// TODO
void bitmap_unmark(uint32_t index) {

}

// TODO
void bitmap_find_free_range(uint32_t index, size_t length) {

}

// TODO
void bitmap_find_free() {
  
}

// TODO
bool bitmap_isset(uint32_t index) {

  return false;
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
