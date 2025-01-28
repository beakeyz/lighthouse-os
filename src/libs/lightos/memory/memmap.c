#include "memmap.h"
#include "lightos/api/handle.h"
#include "lightos/api/memory.h"
#include "lightos/handle.h"
#include "lightos/syscall.h"
#include "stdlib.h"

error_t vmem_map(HANDLE handle, void** presult, void* addr, size_t len, u32 flags)
{
    void* result = sys_map_vmem(handle, addr, len, flags);

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

/*!
 * @brief: Binds a certain page range to a given vmem object
 */
error_t vmem_bind(HANDLE handle, void* addr, size_t len, u32 flags)
{
    return vmem_map(handle, &addr, addr, len, flags | VMEM_FLAG_BIND);
}

error_t vmem_unbind(HANDLE handle)
{
    /*
     * Prompt the mapping at @handle to be deleted. Returns an error if we
     * don't have permission to remove this mapping, or if there is no mapping present
     */
    return vmem_map(handle, NULL, NULL, NULL, VMEM_FLAG_DELETE);
}

error_t vmem_protect(void* addr, size_t len, u32 flags)
{
    if (!addr || !len)
        return EINVAL;

    return sys_protect_vmem(addr, len, flags);
}

error_t vmem_get_range(HANDLE handle, page_range_t* prange)
{
    if (!prange)
        return -EINVAL;

    /* Read the range into the page range struct */
    return handle_read(handle, 0, prange, sizeof(*prange));
}

error_t vmem_remap(HANDLE handle, const char* name, void** paddr, size_t* plen, u32 flags, HANDLE* pmap)
{
    HANDLE ret;
    void* rstart;
    size_t rsize;
    error_t error;
    page_range_t range;

    error = vmem_get_range(handle, &range);

    /* Could not get a range from this handle? Fuck */
    if (error)
        return -EINVAL;

    ret = HNDL_INVAL;

    /* Maybe create a new mapping */
    if (name)
        ret = vmem_open(name, HF_RW, HNDL_MODE_CREATE_NEW);

    /* Get range bounds */
    rsize = page_range_size(&range);
    /* Clear the start pointer */
    rstart = nullptr;

    /* First, create a new mapping we can use */
    error = vmem_map(handle, &rstart, NULL, rsize, VMEM_FLAG_READ | VMEM_FLAG_WRITE | VMEM_FLAG_SHARED | VMEM_FLAG_REMAP);

    if (error)
        goto close_and_return_err;

    /* If the ret handle is invalid at this point, we can just export the addr and size and return */
    if (IS_OK(handle_verify(ret))) {

        /* Then, map the result of this onto the new vmem object */
        error = vmem_bind(ret, rstart, rsize, flags);

        if (error)
            goto close_and_return_err;
    }

    /* Export the bounds if they are specified */
    if (paddr)
        *paddr = rstart;
    if (plen)
        *plen = rsize;
    /* Return the new vmem object */
    if (pmap)
        *pmap = ret;

    return 0;

close_and_return_err:
    if (handle_verify(ret))
        vmem_close(ret);

    return error;
}

/*!
 * @brief: Opens a virtual memory mapping
 *
 * Virtual memory mappings live inside processes environments as sysvars.
 */
HANDLE vmem_open(const char* path, u32 flags, enum HNDL_MODE mode)
{
    return open_handle(path, HNDL_TYPE_OBJECT, flags, mode);
}

/*!
 *
 */
HANDLE vmem_open_rel(HANDLE handle, const char* path, u32 flags)
{
    return open_handle_from(handle, path, HNDL_TYPE_OBJECT, flags, HNDL_MODE_NORMAL);
}

error_t vmem_close(HANDLE handle)
{
    return close_handle(handle);
}
