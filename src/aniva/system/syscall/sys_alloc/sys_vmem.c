#include "libk/flow/error.h"
#include "libk/math/math.h"
#include "lightos/api/handle.h"
#include "lightos/api/memory.h"
#include "lightos/api/objects.h"
#include "lightos/api/sysvar.h"
#include "lightos/syscall.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sys/types.h"
#include <mem/kmem.h>
#include <system/sysvar/var.h>

static void _apply_memory_flags(uint32_t userflags, uint32_t* customflags, uint32_t* kmem_flags)
{
    u32 kmem = 0;

    if ((userflags & VMEM_FLAG_WRITE) == VMEM_FLAG_WRITE)
        kmem |= KMEM_FLAG_WRITABLE;

    /* This flag gives us shit for some reason */
    // if ((userflags & VMEM_FLAG_EXEC) != VMEM_FLAG_EXEC)
    // kmem |= KMEM_FLAG_NOEXECUTE;

    *customflags = NULL;
    *kmem_flags = kmem;
}

static u32 _get_page_range_flags(u32 vmem_flags)
{
    u32 ret = 0;

    if ((vmem_flags & VMEM_FLAG_SHARED) == VMEM_FLAG_SHARED)
        ret |= PAGE_RANGE_FLAG_EXPORTED;

    if ((vmem_flags & VMEM_FLAG_WRITE) == VMEM_FLAG_WRITE)
        ret |= PAGE_RANGE_FLAG_WRITABLE;

    return ret;
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

/*
 * TODO: Implement object permissions again xD
 */

static inline bool __can_proc_delete_vmem(khandle_t* handle, proc_t* c_proc, proc_t* target_proc, sysvar_t* var)
{
    if ((handle->flags & HNDL_FLAG_W) != HNDL_FLAG_W)
        return false;

    /* Processes can always delete their own memory, but other processes only can if  */
    // return (c_proc == target_proc || pattr_hasperm(&var->obj->attr, &c_proc->m_env->attr, PATTR_WRITE));
    return true;
}

static inline bool __can_proc_map_vmem(khandle_t* handle, proc_t* c_proc, proc_t* target_proc, sysvar_t* var)
{
    /* This handle also needs to have the write flag  */
    if ((handle->flags & HNDL_FLAG_W) != HNDL_FLAG_W)
        return false;

    // return (c_proc == target_proc || (pattr_hasperm(&var->obj->attr, &c_proc->m_env->attr, PATTR_WRITE)));
    return true;
}

static inline bool __can_proc_remap_vmem(khandle_t* handle, proc_t* c_proc, proc_t* target_proc, sysvar_t* var)
{
    /* This handle also needs to have the read flag  */
    if ((handle->flags & HNDL_FLAG_R) != HNDL_FLAG_R)
        return false;

    /* When remapping, we only read the range from the var, hence why we need read permissions */
    // return (c_proc == target_proc || (pattr_hasperm(&var->obj->attr, &c_proc->m_env->attr, PATTR_READ)));
    return true;
}

static inline void* __sys_delete_local_vmem(proc_t* c_proc, void* addr, size_t len)
{
    /* First validate the pointer to make sure it points to a valid address */
    if (kmem_validate_ptr(c_proc, (vaddr_t)addr, len))
        return nullptr;

    /* Simply unmap the specified range */
    if (kmem_user_dealloc(c_proc, (vaddr_t)addr, len))
        return nullptr;

    /* Just return addr at this point and hope no one touches it anymore */
    return addr;
}

static void* __sys_delete_vmem(khandle_t* khandle, proc_t* c_proc, void* addr, size_t len, u32 flags)
{
    void* ret;
    sysvar_t* var;
    proc_t* target_proc;
    page_range_t range = { 0 };

    if (!khandle)
        return __sys_delete_local_vmem(c_proc, addr, len);

    /* Grab the pertaining sysvar holding the vmem object */
    var = oss_object_unwrap(khandle->object, OT_SYSVAR);

    /* Invalid sysvar =( */
    if (!var || var->type != SYSVAR_TYPE_MEM_RANGE)
        return nullptr;

    /* Get the environments process */
    target_proc = oss_object_unwrap(var->parent, OT_PROCESS);

    /* Invalid probably */
    if (!target_proc)
        return nullptr;

    /* Check if our process can act on the target */
    if (!__can_proc_delete_vmem(khandle, c_proc, target_proc, var))
        return nullptr;

    /* Pre-set the return variable */
    ret = NULL;

    /* Lock the variable just in case */
    sysvar_lock(var);

    /* Read the range from the var */
    if (sysvar_read(var, &range, sizeof(range)))
        goto unlock_and_exit;

    /* Deallocate the range */
    if (kmem_user_dealloc(target_proc, (vaddr_t)page_range_to_ptr(&range), page_range_size(&range)))
        goto unlock_and_exit;

    /* Clear the range block */
    memset(&range, 0, sizeof(range));

    /*
     * Write back the new range
     * FIXME: What do we do when this call fails and we have already deallocated the range?
     */
    (void)sysvar_write(var, &range, sizeof(range));

    /* Success. Set the return variable to indicate that */
    ret = addr;
unlock_and_exit:
    /* Unlock the var */
    sysvar_unlock(var);

    return ret;
}

static void* __sys_remap_local_vmem(proc_t* c_proc, void* addr, size_t len, u32 flags)
{
    /* TODO: Implement remaps without handles */
    kernel_panic("__sys_remap_vmem: See todo");
}

static void* __sys_remap_vmem(khandle_t* khandle, proc_t* c_proc, void* addr, size_t len, u32 flags)
{
    void* result = nullptr;
    sysvar_t* var;
    proc_t* target_proc;
    page_range_t range = { 0 };
    vaddr_t target_addr;
    size_t target_size;
    u32 custom_flags, page_flags;

    /* We need a size specified */
    if (!len)
        return nullptr;

    if (!khandle)
        return __sys_remap_local_vmem(c_proc, addr, len, flags);

    /* Yes; we'll have to check if we can even map in this vmem object */
    var = oss_object_unwrap(khandle->object, OT_SYSVAR);

    /* Invalid sysvar =( */
    if (!var || var->type != SYSVAR_TYPE_MEM_RANGE)
        return nullptr;

    /* Get the process accociated with it */
    target_proc = oss_object_unwrap(var->parent, OT_PROCESS);

    /* Probably inval */
    if (!target_proc)
        return nullptr;

    /* Can't remap. Dip */
    if (!__can_proc_remap_vmem(khandle, c_proc, target_proc, var))
        return nullptr;

    /* Read the range from the sysvar */
    if (sysvar_read(var, &range, sizeof(range)))
        return nullptr;

    /* Either of these being NULL implies an unused range */
    if (!range.nr_pages || !range.refc)
        return nullptr;

    /* Get the range bounds */
    target_addr = (vaddr_t)page_range_to_ptr(&range);
    target_size = MIN(page_range_size(&range), len);

    /* Get page flags from the vmem flags */
    _apply_memory_flags(flags, &custom_flags, &page_flags);

    /* Reallocate */
    if (kmem_user_realloc(&result, c_proc, target_proc, target_addr, target_size, custom_flags, page_flags))
        /* Reset result just to be safe */
        result = nullptr;

    /* TODO: Remap the specified range */
    return result;
}

/*!
 * @brief: Map local virtual memory
 *
 * There was no (valid) handle specified so we'll just map some memory
 */
static void* __sys_map_local_vmem(proc_t* c_proc, void* addr, size_t len, u32 flags)
{
    error_t error;
    u32 custom_flags, page_flags;

    /* Get page flags from the vmem flags */
    _apply_memory_flags(flags, &custom_flags, &page_flags);

    /* Try to allocate */
    if (addr)
        error = kmem_user_alloc_scattered(&addr, c_proc, (vaddr_t)addr, len, custom_flags, page_flags);
    else
        error = kmem_user_alloc_range(&addr, c_proc, len, custom_flags, page_flags);

    /* Shitt */
    if (error)
        return nullptr;

    return addr;
}

static void* __sys_map_vmem(khandle_t* khandle, proc_t* c_proc, void* addr, size_t len, u32 flags)
{
    sysvar_t* var;
    proc_t* target_proc;
    page_range_t range = { 0 };

    /* No length at this point is fatal */
    if (!len)
        return nullptr;

    /* Do we have a handle? */
    if (!khandle)
        /* No; lets just do a private allocation inside the process */
        return __sys_map_local_vmem(c_proc, addr, len, flags);

    /* Yes; we'll have to check if we can even map in this vmem object */
    var = oss_object_unwrap(khandle->object, OT_SYSVAR);

    /* Invalid sysvar =( */
    if (!var || var->type != SYSVAR_TYPE_MEM_RANGE)
        return nullptr;

    /* Get the process accociated with it */
    target_proc = oss_object_unwrap(var->parent, OT_PROCESS);

    /* Can't map. Dip */
    if (!__can_proc_map_vmem(khandle, c_proc, target_proc, var))
        return nullptr;

    /* Lock the variable so no one fucks us */
    sysvar_lock(var);

    /* Read the range from the sysvar */
    sysvar_read(var, &range, sizeof(range));

    /* If the range is alredy set, we can't map in it =( */
    if (range.page_idx || range.nr_pages || range.refc) {
        addr = nullptr;
        goto unlock_and_exit;
    }

    /* Do a 'local' map in the target process */
    addr = __sys_map_local_vmem(target_proc, addr, len, flags);

    if (!addr)
        goto unlock_and_exit;

    /* Init this range */
    init_page_range(&range, kmem_get_page_idx((vaddr_t)addr), GET_PAGECOUNT((u64)addr, len), NULL, 1);

    /* Write back the new range */
    (void)sysvar_write(var, &range, sizeof(range));

unlock_and_exit:
    sysvar_unlock(var);

    return addr;
}

static void* __sys_bind_vmem(khandle_t* khandle, proc_t* c_proc, void* addr, size_t len, u32 flags)
{
    sysvar_t* var;
    proc_t* target_proc;
    page_range_t range = { 0 };

    /* Can't bind to an invalid handle */
    if (!khandle)
        return nullptr;

    if (!addr || !len)
        return nullptr;

    /* Get the reference */
    var = oss_object_unwrap(khandle->object, OT_SYSVAR);

    /* Shit */
    if (!var || var->type != SYSVAR_TYPE_MEM_RANGE)
        return nullptr;

    /* Get this */
    target_proc = oss_object_unwrap(var->parent, OT_PROCESS);

    /* Probably invalid */
    if (!target_proc)
        return nullptr;

    /* For binding, the same privileges are needed as for mapping */
    if (!__can_proc_map_vmem(khandle, c_proc, target_proc, var))
        return nullptr;

    sysvar_lock(var);

    /* Read the current state of the sysvar */
    (void)sysvar_read(var, &range, sizeof(range));

    /* We can't implicitly 'rebind' vmem object. The user first has to explicitly unbind */
    if (range.page_idx || range.nr_pages || range.refc)
        goto unlock_and_exit_err;

    /* Initalize the page range */
    init_page_range(&range, kmem_get_page_idx((vaddr_t)addr), GET_PAGECOUNT((vaddr_t)addr, len), _get_page_range_flags(flags), 1);

    /* Write back the new range */
    sysvar_write(var, &range, sizeof(range));

    /* Unlock */
    sysvar_unlock(var);

    /* Return back our address */
    return addr;

unlock_and_exit_err:
    sysvar_unlock(var);
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
    khandle_t* khandle;
    proc_t* c_proc;

    /* No length, no mapping =/ */
    if (!len)
        return nullptr;

    /* Grab the current process */
    c_proc = get_current_proc();

    /* Try to find a handle */
    khandle = find_khandle(&c_proc->m_handle_map, handle);

    /* Check for invalid handle */
    if (khandle && (khandle->type != HNDL_TYPE_OBJECT))
        return nullptr;

    switch (flags & (VMEM_FLAG_REMAP | VMEM_FLAG_DELETE | VMEM_FLAG_BIND)) {
    case VMEM_FLAG_DELETE:
        return __sys_delete_vmem(khandle, c_proc, addr, len, flags);
    case VMEM_FLAG_REMAP:
        return __sys_remap_vmem(khandle, c_proc, addr, len, flags);
    case VMEM_FLAG_BIND:
        return __sys_bind_vmem(khandle, c_proc, addr, len, flags);
    case 0:
        return __sys_map_vmem(khandle, c_proc, addr, len, flags);
    default:
        return nullptr;
    }

    return nullptr;
}

error_t sys_protect_vmem(void* addr, size_t len, u32 flags)
{
    kernel_panic("TODO: Implement syscall (sys_protect_vmem)");
    return 0;
}
