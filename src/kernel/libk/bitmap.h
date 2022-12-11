#ifndef __LIGHT_BITMAP__ 
#define __LIGHT_BITMAP__
#include <libk/stddef.h>

typedef void (*BITMAP_MARK) (
  uint32_t index
);

typedef void (*BITMAP_UNMARK) (
  uint32_t index
);

typedef bool (*BITMAP_ISSET) (
  uint32_t index
);

typedef void (*BITMAP_FIND_FREE) (
);

typedef void (*BITMAP_FIND_FREE_RANGE) (
  uint32_t index,
  size_t length
);

// 64-bit bitmap
typedef struct {
  const uint8_t m_default;
  size_t m_size;
  uint8_t* m_map;

  BITMAP_MARK fMark;
  BITMAP_UNMARK fUnmark;
  BITMAP_ISSET fIsSet;
  BITMAP_FIND_FREE fFindFree;
  BITMAP_FIND_FREE_RANGE fFindFreeRange;
} bitmap_t;

bitmap_t init_bitmap(size_t size);
void bitmap_mark(uint32_t index);
void bitmap_unmark(uint32_t index);

bool bitmap_isset(uint32_t index);

void bitmap_find_free();
void bitmap_find_free_range(uint32_t index, size_t length);

#endif // !
