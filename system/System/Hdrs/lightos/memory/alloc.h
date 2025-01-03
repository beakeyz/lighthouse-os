#ifndef __LIGHTENV_MEM_ALLOC__
#define __LIGHTENV_MEM_ALLOC__

/*
 * lightos memory allocator
 *
 * This allocator gets initialized right before the process starts execution in
 * order to have a large memory pool ready to allocate in.
 * The process may:
 *  - Ask the allocator for a new large memory pool
 *  - Allocate in any existing pool
 *  - Invoke the deallocator from the allocator
 *  - Ask the allocator to allocate in another process (for that we use handles and 'allocator sharing')
 */

#include <lightos/types.h>
#include "sys/types.h"

#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

/*
 * Try to allocate a new pool for this process
 * The pool address is rounded up to the next (Most likely 4Kib) page
 * aligned address. if pooladdr is NULL,
 *
 * if the poolsize is less than MIN_POOLSIZE, the actual
 * size gets rounded up to a multiple of MIN_POOLSIZE
 *
 * Flags indicate the protection level
 */
VOID* allocate_vmem(size_t len, u32 flags);
error_t deallocate_vmem(VOID* addr, size_t len);

#endif // !__LIGHTENV_MEM_ALLOC__
