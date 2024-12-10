#include "memmap.h"
#include "lightos/handle_def.h"
#include "lightos/memory/memflags.h"
#include "lightos/syscall.h"

error_t map_vmem(HANDLE handle, void** presult, void* addr, size_t len, u32 flags)
{
    void* result;

    if (!addr || !len)
        return EINVAL;

    result = sys_map_vmem(handle, addr, len, flags);

    if (!result)
        return -ENOMEM;

    if (presult)
        *presult = result;

    return 0;
}

error_t unmap_vmem(void* addr, size_t len)
{
    /* Simply call the map function with the delete flag */
    return map_vmem(HNDL_INVAL, NULL, addr, len, VMEM_FLAG_DELETE);
}

error_t protect_vmem(void* addr, size_t len, u32 flags)
{
    if (!addr || !len)
        return EINVAL;

    return sys_protect_vmem(addr, len, flags);
}
