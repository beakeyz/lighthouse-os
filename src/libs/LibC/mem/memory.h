#ifndef __LIGHTENV_LIBC_MEMORY__
#define __LIGHTENV_LIBC_MEMORY__

/*
 * Initialize the memory allocation submodule
 */

#include <stddef.h>

void init_memalloc(
  void
);

/*
 * Memory allocation on a byte alignment
 */
void* mem_alloc(
  size_t        size,
  uint32_t      flags
);

/*
 * Memory deallocation 
 */
int mem_dealloc(
  void* addr
);

#endif // !__LIGHTENV_LIBC_MEMORY__
