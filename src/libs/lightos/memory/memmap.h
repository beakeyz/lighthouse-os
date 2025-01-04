#ifndef __LIGHTENV_MEMMAP__
#define __LIGHTENV_MEMMAP__

/*
 * lightos memory mapper
 *
 * Extention of the allocator which pulls memory from files or any location
 * other than ram and maps it into workram
 */

#include <lightos/types.h>
#include "lightos/handle_def.h"
#include "lightos/memory/memory.h"

/*
 * Map a section of physical memory to a virtual range
 * The range inbetween the virtual_base and size must be inside a pool that is alloced
 * by or for this process.
 *
 * Let @handle be HNDL_INVAL in order to map inside our own address space
 */
error_t vmem_map(HANDLE handle, void** presult, void* addr, size_t len, u32 flags);
error_t vmem_unmap(void* addr, size_t len);
error_t vmem_protect(void* addr, size_t len, u32 flags);
error_t vmem_get_mapping(HANDLE handle, page_range_t* prange);

HANDLE vmem_open(const char* path, u32 flags, enum HNDL_MODE mode);
HANDLE vmem_open_rel(HANDLE handle, const char* path, u32 flags);
error_t vmem_close(HANDLE handle);

#endif // !__LIGHTENV_MEMMAP__
