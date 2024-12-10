#ifndef __LIGHTENV_MEMMAP__
#define __LIGHTENV_MEMMAP__

/*
 * lightos memory mapper
 *
 * Extention of the allocator which pulls memory from files or any location
 * other than ram and maps it into workram
 */

#include "lightos/error.h"
#include "lightos/handle_def.h"

/*
 * Map a section of physical memory to a virtual range
 * The range inbetween the virtual_base and size must be inside a pool that is alloced
 * by or for this process.
 *
 * Let @handle be HNDL_INVAL in order to map inside our own address space
 */
error_t map_vmem(HANDLE handle, void** presult, void* addr, size_t len, u32 flags);
error_t unmap_vmem(void* addr, size_t len);

error_t protect_vmem(void* addr, size_t len, u32 flags);

#endif // !__LIGHTENV_MEMMAP__
