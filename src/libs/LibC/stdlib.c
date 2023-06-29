
#include <mem/memory.h>
#include "stdlib.h"

/*
 * Allocate a block of memory on the heap
 */
void* malloc(size_t __size)
{
  return mem_alloc(__size, MEM_MALLOC);
}

/*
 * Allocate count blocks of memory of __size bytes
 */
void* calloc(size_t count, size_t __size)
{
  if (!count)
    return nullptr;

  return mem_alloc(count * __size, MEM_MALLOC);
}

/*
 * Reallocate the block at ptr of __size bytes
 */
void* realloc(void* ptr, size_t __size)
{
  if (!__size) {
    free(ptr);
    return nullptr;
  }

  return mem_move_alloc(ptr, __size, MEM_MALLOC);
}

/*
 * Free a block of memory allocated by malloc, calloc or realloc
 */
void free(void* ptr)
{
  mem_dealloc(ptr, MEM_MALLOC);
}

