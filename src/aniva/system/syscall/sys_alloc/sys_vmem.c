#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "lightos/memory/memory.h"
#include "lightos/syscall.h"
#include "logging/log.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <mem/kmem.h>
#include <system/sysvar/var.h>

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

static inline bool __can_map_in_handle(khandle_t* khandle)
{
    return (khandle->type == HNDL_TYPE_VMEM);
}

static inline u32 __get_page_range_flags(u32 vmem_flags)
{
    u32 ret = 0;

    /* Shared region, mark it as such */
    if ((vmem_flags & VMEM_FLAG_SHARED) == VMEM_FLAG_SHARED)
        ret |= PAGE_RANGE_FLAG_EXPORTED;

    return ret;
}

static void* __sys_map_vmem_in_handle(khandle_t* khandle, proc_t* c_proc, void* addr, size_t len, u32 vmem_flags, u32 custom_flags, u32 kmem_flags)
{
    error_t error = EOK;
    u32 page_range_flags;
    u32 nr_pages;
    page_range_t range;

    /* This is just sad =( */
    if (!__can_map_in_handle(khandle))
        return nullptr;

    KLOG_DBG("Trying to map into handle: %s\n", khandle->reference.vmem_range->key);

    /* Lock the sysvar */
    sysvar_lock(khandle->reference.vmem_range);

    /* Read the current range from the handle */
    error = sysvar_read(khandle->reference.pvar, &range, sizeof(range));

    /* Well that sucks */
    if (error)
        goto unlock_and_exit;

    KLOG_DBG("Current : %d\n", range.nr_pages);

    /* Check if there is already an active mapping in this range and remove it if there is */
    if (range.nr_pages && range.refc) {
        error = kmem_user_dealloc(c_proc, kmem_get_page_addr(range.page_idx), range.nr_pages << 12);

        /* Well that sucks */
        if (error)
            goto unlock_and_exit;
    }

    /* Get some data for the new mapping */
    page_range_flags = __get_page_range_flags(vmem_flags);
    nr_pages = GET_PAGECOUNT((u64)addr, len);

    /* First allocate a range */
    if (addr)
        error = kmem_user_alloc_scattered(&addr, c_proc, (vaddr_t)addr, len, custom_flags, kmem_flags);
    else
        error = kmem_user_alloc_range(&addr, c_proc, len, custom_flags, kmem_flags);

    /* Check if the allocation went OK */
    if (error || !addr)
        goto unlock_and_exit;

    /* Initialize the new range */
    init_page_range(&range, kmem_get_page_idx((u64)addr), nr_pages, page_range_flags, 1);

    /* Write back the new range into the handle */
    sysvar_write(khandle->reference.pvar, &range, sizeof(range));

    /* Unlock the sysvar to the rest of the world =D */
    sysvar_unlock(khandle->reference.vmem_range);

    /* Convert this shit to a pointer */
    return page_range_to_ptr(&range);

unlock_and_exit:
    sysvar_unlock(khandle->reference.pvar);

    return nullptr;
}

/*!
 * @brief: Map memory to handles
 *
 * if @handle is HNDL_INVAL, we simply map inside the current processes address space
 *
 * @returns: The resulting pointer to the mapping. NULL if there was an error
 */
void* sys_map_vmem(HANDLE handle, void* addr, size_t len, u32 flags)
{
    void* ret;
    error_t error;
    khandle_t* khandle;
    u32 customflags;
    u32 kmem_flags;
    proc_t* c_proc;

    /* No length, no mapping =/ */
    if (!len)
        return nullptr;

    /* Grab the current process */
    c_proc = get_current_proc();

    /* Get the memory flags for kmem */
    _apply_memory_flags(flags, &customflags, &kmem_flags);

    /* Try to find a handle */
    khandle = find_khandle(&c_proc->m_handle_map, handle);

    /* If there was a handle specified, try to map into this handle */
    if (khandle)
        return __sys_map_vmem_in_handle(khandle, c_proc, addr, len, flags, customflags, kmem_flags);

    /* Try to do a normal mapping */
    if (!addr)
        error = kmem_user_alloc_range(&ret, c_proc, len, customflags, kmem_flags);
    else
        error = kmem_user_alloc_scattered(&ret, c_proc, (vaddr_t)addr, len, customflags, kmem_flags);

    if (error)
        return NULL;

    return ret;
}

error_t sys_protect_vmem(void* addr, size_t len, u32 flags)
{
    kernel_panic("TODO: Implement syscall (sys_protect_vmem)");
    return 0;
}
