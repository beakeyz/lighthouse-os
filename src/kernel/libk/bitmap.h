#ifndef __LIGHT_BITMAP__ 
#define __LIGHT_BITMAP__
#include <libk/stddef.h>

struct bitmap;

typedef void (*BITMAP_MARK) (
  struct bitmap* this,
  uint32_t index
);

typedef void (*BITMAP_UNMARK) (
  struct bitmap* this,
  uint32_t index
);

typedef bool (*BITMAP_ISSET) (
  struct bitmap* this,
  uint32_t index
);

typedef uintptr_t (*BITMAP_FIND_FREE) (
  struct bitmap* this
);

typedef uintptr_t (*BITMAP_FIND_FREE_RANGE) (
  struct bitmap* this,
  uint32_t index,
  size_t length
);

// 8-bit bitmap
typedef struct bitmap {
  const uint8_t m_default;
  size_t m_size; // size in bytes
  size_t m_entries; // size in bits
  uint8_t* m_map;

  BITMAP_MARK fMark;
  BITMAP_UNMARK fUnmark;
  BITMAP_ISSET fIsSet;
  BITMAP_FIND_FREE fFindFree;
  BITMAP_FIND_FREE_RANGE fFindFreeRange;
} bitmap_t;

bitmap_t init_bitmap(size_t size);
void bitmap_mark(bitmap_t* this, uint32_t index);
void bitmap_unmark(bitmap_t* this, uint32_t index);

bool bitmap_isset(bitmap_t* this, uint32_t index);

uintptr_t bitmap_find_free(bitmap_t* this);
uintptr_t bitmap_find_free_range(bitmap_t* this, uint32_t index, size_t length);

#endif // !
