#include "libk/flow/error.h"
#include "lightos/memory/memflags.h"
#include "lightos/syscall.h"
#include "logging/log.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <mem/kmem.h>

static void _apply_memory_flags(uint32_t userflags, uint32_t* customflags, uint32_t* kmem_flags)
{
    u32 kmem = 0;

    if ((userflags & VMEM_FLAG_WRITE) == VMEM_FLAG_WRITE)
        kmem |= KMEM_FLAG_WRITABLE;

    /* This flag gives us shit for some reason */
    //if ((userflags & VMEM_FLAG_EXEC) != VMEM_FLAG_EXEC)
        //kmem |= KMEM_FLAG_NOEXECUTE;

    *customflags = NULL;
    *kmem_flags = kmem;
}

/*
 * Allocate a range of user pages
 * TODO: check the user buffer
 */
error_t sys_alloc_vmem(size_t size, u32 flags, vaddr_t* buffer)
{
    int error;
    uint32_t customflags;
    uint32_t kmem_flags;
    void* result;
    proc_t* current_process;

    if (!size || !buffer)
        return EINVAL;

    /* Align up the size to the closest next page base */
    size = ALIGN_UP(size, SMALL_PAGE_SIZE);

    current_process = get_current_proc();

    if (!current_process)
        return EINVAL;

    /* Translate user flags into kernel flags */
    _apply_memory_flags(flags, &customflags, &kmem_flags);

    /* TODO: Must calls in syscalls that fail may kill the process with the internal error flags set */
    error = kmem_user_alloc_range(&result, current_process, size, customflags, kmem_flags);

    if (error)
        return ENOMEM;

    /* Set the buffer hihi */
    *buffer = (vaddr_t)result;
    return 0;
}

error_t sys_dealloc_vmem(vaddr_t buffer, size_t size)
{
    kernel_panic("TODO: Implement syscall (sys_dealloc_vmem)");
    return 0;
}

void* sys_map_vmem(HANDLE handle, void* addr, size_t len, u32 flags)
{
    kernel_panic("TODO: Implement syscall (sys_map_vmem)");
    return 0;
}

error_t sys_protect_vmem(void* addr, size_t len, u32 flags)
{
    kernel_panic("TODO: Implement syscall (sys_protect_vmem)");
    return 0;
}
