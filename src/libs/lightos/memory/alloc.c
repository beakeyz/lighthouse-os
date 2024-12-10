#include "alloc.h"
#include "lightos/memory/memflags.h"
#include "lightos/syscall.h"

/*!
 * @brief: Simply asks the kernel for memory
 *
 * NOTE: These allocations might give nullptrs, but for this reason we can
 * check the @poolsize buffer to see if that is non-null
 */
VOID* allocate_vmem(size_t len, u32 flags)
{
    u64 result;

    if (!len)
        return NULL;

    len = ALIGN_UP(len, MEMPOOL_ALIGN);

    /* Request memory from the system */
    if (sys_alloc_vmem(len, flags, &result))
        return NULL;

    return (VOID*)result;
}

error_t deallocate_vmem(VOID* addr, size_t len)
{
    return sys_dealloc_vmem((vaddr_t)addr, len);
}
