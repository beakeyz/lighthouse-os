#ifndef __LIGHTENV_LIBC_MEMORY__
#define __LIGHTENV_LIBC_MEMORY__

/*
 * Initialize the memory allocation submodule
 */

#include <stdint.h>

/*
 * Memory allocation on a byte alignment
 */
void* mem_alloc(
    size_t size);

/*
 * Move the allocation at a certain address to a bigger buffer
 * returns the old pointer if the size didn't change
 */
void* mem_move_alloc(
    void* ptr,
    size_t new_size);

/*
 * Memory deallocation
 */
int mem_dealloc(
    void* addr);

#endif // !__LIGHTENV_LIBC_MEMORY__
