#ifndef __LIGHTENV_LIBC_MEMORY__
#define __LIGHTENV_LIBC_MEMORY__

/*
 * Initialize the memory allocation submodule
 */

#include <stddef.h>

/* Allocate malloc memory */
#define MEM_MALLOC      (0x00000001)

/*
 * Memory allocation on a byte alignment
 */
void* mem_alloc(
  size_t        size,
  uint32_t      flags
);

/*
 * Move the allocation at a certain address to a bigger buffer
 * returns the old pointer if the size didn't change
 */
void* mem_move_alloc(
  void* ptr,
  size_t new_size,
  uint32_t flags
);

/*
 * Memory deallocation 
 */
int mem_dealloc(
  void* addr,
  uint32_t flags
);

#endif // !__LIGHTENV_LIBC_MEMORY__
