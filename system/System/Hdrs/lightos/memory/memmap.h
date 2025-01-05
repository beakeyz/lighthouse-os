#ifndef __LIGHTENV_MEMMAP__
#define __LIGHTENV_MEMMAP__

/*
 * lightos memory mapper
 *
 * Extention of the allocator which pulls memory from files or any location
 * other than ram and maps it into workram
 *
 * Some rules we apply to virtual memory mappings:
 * 1) Once a range has been bounded to a mapping (pointed to by a handle), it can't be changed
 *    until the range is unbound, by a call to vmem_unbind. This call will remove the current
 *    range on te mapping and export it if @paddr and @plen are specified or delete it if they aren't
 * 2) Calling vmem_map with the VMEM_FLAG_DELETE flag will prompt unmapping. In the case where there
 *    is a valid handle specified, the address and length fields are ignored. If there is no or
 *    an invalid handle given, the specified address will be unmapped.
 */

#include "lightos/handle_def.h"
#include "lightos/memory/memory.h"
#include <lightos/lightos.h>
#include <lightos/types.h>

/*!
 * Map a section of physical memory to a virtual range
 * The range inbetween the virtual_base and size must be inside a pool that is alloced
 * by or for this process.
 *
 * Let @handle be HNDL_INVAL in order to map inside our own address space
 *
 * If @handle is specified, there are two modes of operation:
 * 1) Both @addr and @len are set: We're going to create a new mapping on @handle and
 *    the old one will be deleted and unmapped
 * 2) @addr is cleared and @len is set: We're going to remap the mapping in @handle
 *    inside our own addresspace
 */
LEXPORT error_t vmem_map(HANDLE handle, void** presult, void* addr, size_t len, u32 flags);
LEXPORT error_t vmem_unmap(void* addr, size_t len);
LEXPORT error_t vmem_bind(HANDLE handle, void* addr, size_t len, u32 flags);
LEXPORT error_t vmem_unbind(HANDLE handle);
LEXPORT error_t vmem_protect(void* addr, size_t len, u32 flags);
LEXPORT error_t vmem_get_range(HANDLE handle, page_range_t* prange);

/*!
 * @brief: Remap the mapping that lives at @handle to ourselves
 *
 * @handle: The handle where we need to map from
 * @name: The name the new mapping must get
 * @paddr: Buffer for the address we get this mapping at
 * @plen:
 */
LEXPORT error_t vmem_remap(HANDLE handle, const char* name, void** paddr, size_t* plen, u32 flags, HANDLE* pmap);

LEXPORT HANDLE vmem_open(const char* path, u32 flags, enum HNDL_MODE mode);
LEXPORT HANDLE vmem_open_rel(HANDLE handle, const char* path, u32 flags);
LEXPORT error_t vmem_close(HANDLE handle);

#endif // !__LIGHTENV_MEMMAP__
