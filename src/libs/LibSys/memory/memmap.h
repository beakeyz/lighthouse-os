#ifndef __LIGHTENV_MEMMAP__
#define __LIGHTENV_MEMMAP__

/*
 * LibSys memory mapper
 *
 * Extention of the allocator which pulls memory from files or any location 
 * other than ram and maps it into workram
 */

#include "LibSys/handle_def.h"
#include "LibSys/system.h"

/*
 * Map a section of physical memory to a virtual range
 * The range inbetween the virtual_base and size must be inside a pool that is alloced 
 * by or for this process.
 */
int map_process_memory(
  __IN__ QWORD virtual_base,
  __IN__ size_t size,
  __OUT__ size_t* bytes_mapped
);

/*
 * Advanced. Map a section of physical memory to a virtual range with extra options
 * for mapping in another process, or applying flags to the mapping
 */
int map_process_memory_av(
  __IN__ __OPTIONAL__ HANDLE_t handle,
  __IN__ __OPTIONAL__ DWORD flags,
  __IN__ QWORD virtual_base,
  __IN__ size_t size,
  __OUT__ size_t* bytes_mapped
);

#endif // !__LIGHTENV_MEMMAP__
