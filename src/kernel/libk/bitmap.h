#ifndef __LIGHT_BITMAP__ 
#define __LIGHT_BITMAP__
#include <libk/stddef.h>

// 64-bit bitmap
typedef struct {
  uint64_t* m_map;
} bitmap_t;

void bitmap_init(size_t size);
void bitmap_mark(uint32_t index);
void bitmap_unmark(uint32_t index);

#endif // !
