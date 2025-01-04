#include "memmap.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/memory/memory.h"
#include "lightos/syscall.h"

error_t vmem_map(HANDLE handle, void** presult, void* addr, size_t len, u32 flags)
{
    void* result;

    if (!len)
        return -EINVAL;

    result = sys_map_vmem(handle, addr, len, flags);

    if (!result)
        return -ENOMEM;

    if (presult)
        *presult = result;

    return 0;
}

error_t vmem_unmap(void* addr, size_t len)
{
    /* Simply call the map function with the delete flag */
    return vmem_map(HNDL_INVAL, NULL, addr, len, VMEM_FLAG_DELETE);
}

error_t vmem_protect(void* addr, size_t len, u32 flags)
{
    if (!addr || !len)
        return EINVAL;

    return sys_protect_vmem(addr, len, flags);
}

error_t vmem_get_mapping(HANDLE handle, page_range_t* prange)
{
    if (!prange)
        return -EINVAL;

    return handle_read(handle, prange, sizeof(*prange));
}

/*!
 * @brief: Opens a virtual memory mapping
 *
 * Virtual memory mappings live inside processes environments as sysvars.
 */
HANDLE vmem_open(const char* path, u32 flags, enum HNDL_MODE mode)
{
    return open_handle(path, HNDL_TYPE_VMEM, flags, mode);
}

/*!
 * 
 */
HANDLE vmem_open_rel(HANDLE handle, const char* path, u32 flags)
{
    return open_handle_rel(handle, path, HNDL_TYPE_VMEM, flags, HNDL_MODE_NORMAL);
}

error_t vmem_close(HANDLE handle)
{
    return close_handle(handle);
}
