#include "sys_alloc_mem.h"
#include "libk/flow/error.h"
#include "lightos/memory/memflags.h"
#include "lightos/syscall.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <mem/kmem_manager.h>

static inline void _apply_memory_flags(uint32_t userflags, uint32_t* customflags, uint32_t* kmem_flags)
{
    *customflags = NULL;
    *kmem_flags = NULL;

    if ((userflags & MEMPOOL_FLAG_W) == MEMPOOL_FLAG_W)
        *kmem_flags |= KMEM_FLAG_WRITABLE;
}

/*
 * Allocate a range of user pages
 * TODO: check the user buffer
 */
uint32_t sys_alloc_page_range(size_t size, uint32_t flags, void* __user* buffer)
{
    uint32_t customflags;
    uint32_t kmem_flags;
    proc_t* current_process;

    if (!size || !buffer)
        return SYS_INV;

    /* Align up the size to the closest next page base */
    size = ALIGN_UP(size, SMALL_PAGE_SIZE);

    current_process = get_current_proc();

    if (!current_process || IsError(kmem_validate_ptr(current_process, (uintptr_t)buffer, 1)))
        return SYS_INV;

    /* Translate user flags into kernel flags */
    _apply_memory_flags(flags, &customflags, &kmem_flags);

    /* TODO: Must calls in syscalls that fail may kill the process with the internal error flags set */
    *buffer = (void*)Must(kmem_user_alloc_range(current_process, size, customflags, kmem_flags));

    return SYS_OK;
}
