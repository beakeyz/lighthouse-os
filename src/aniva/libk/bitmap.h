#ifndef __ANIVA_BITMAP__ 
#define __ANIVA_BITMAP__
#include "libk/error.h"
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

typedef void (*BITMAP_MARK_RANGE) (
  struct bitmap* this,
  uint32_t index,
  size_t length
);

typedef void (*BITMAP_UNMARK_RANGE) (
  struct bitmap* this,
  uint32_t index,
  size_t length
);

typedef bool (*BITMAP_ISSET) (
  struct bitmap* this,
  uint32_t index
);

typedef ErrorOrPtr (*BITMAP_FIND_FREE) (
  struct bitmap* this
);

typedef ErrorOrPtr (*BITMAP_FIND_FREE_RANGE) (
  struct bitmap* this,
  size_t length
);

// 8-bit bitmap
typedef struct bitmap {
  uint8_t m_default;
  size_t m_size; // size in bytes
  size_t m_entries; // size in bits
  uint8_t* m_map;

  BITMAP_MARK fMark;
  BITMAP_UNMARK fUnmark;
  BITMAP_MARK_RANGE fMarkRange;
  BITMAP_UNMARK_RANGE fUnmarkRange;
  BITMAP_ISSET fIsSet;
  BITMAP_FIND_FREE fFindFree;
  BITMAP_FIND_FREE_RANGE fFindFreeRange;
} bitmap_t;

bitmap_t init_bitmap(size_t size);
bitmap_t init_bitmap_with_default(size_t size, uint8_t default_value);

void bitmap_mark(bitmap_t* this, uint32_t index);
void bitmap_unmark(bitmap_t* this, uint32_t index);

bool bitmap_isset(bitmap_t* this, uint32_t index);

ErrorOrPtr bitmap_find_free(bitmap_t* this);
ErrorOrPtr bitmap_find_free_range(bitmap_t* this, size_t length);

void bitmap_mark_range(bitmap_t* this, uint32_t index, size_t length);
void bitmap_unmark_range(bitmap_t* this, uint32_t index, size_t length);

#endif // !
