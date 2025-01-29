#include "lightos/api/handle.h"
#include "lightos/api/sysvar.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/sysvar/map.h"
#include "system/sysvar/var.h"

enum SYSVAR_TYPE sys_get_sysvar_type(HANDLE pvar_handle)
{
    sysvar_t* var;
    khandle_t* handle;
    proc_t* current_proc;

    current_proc = get_current_proc();

    if (!current_proc)
        return SYSVAR_TYPE_INVAL;

    /* Find the khandle */
    handle = find_khandle(&current_proc->m_handle_map, pvar_handle);

    if (!handle)
        return SYSVAR_TYPE_INVAL;

    /* Extract the profile variable */
    var = oss_object_unwrap(handle->object, OT_SYSVAR);

    if (!var)
        return SYSVAR_TYPE_INVAL;

    /*
     * Set the userspace buffer
     * TODO: create a kmem routine that handles memory opperations to and from
     * userspace
     */
    return var->type;
}

/*!
 * @brief: Syscall entry for the creation of profile variables
 *
 * NOTE: this syscall kinda assumes that the caller has permission to create variables on this profile if
 * it managed to get a handle with write access. This means that permissions need to be managed correctly
 * by sys_open in that regard. See the TODO comment inside sys_open
 */
HANDLE sys_create_sysvar(const char* key, handle_flags_t flags, enum SYSVAR_TYPE type, void* buffer, size_t len)
{
    proc_t* proc;
    sysvar_t* var;
    HANDLE ret;
    khandle_t* target_khandle;
    khandle_t new_khandle;
    oss_object_t* target_object;

    proc = get_current_proc();

    if ((kmem_validate_ptr(proc, (uintptr_t)key, 1)))
        return HNDL_INVAL;

    if ((kmem_validate_ptr(proc, (uintptr_t)buffer, 1)))
        return HNDL_INVAL;

    target_khandle = find_khandle(&proc->m_handle_map, flags.s_rel_hndl);

    /* Invalid handle =/ */
    if (!target_khandle)
        return HNDL_INVAL;

    /* Can't write to this handle =/ */
    if ((target_khandle->flags & HF_W) != HF_W)
        return HNDL_INVAL;

    target_object = target_khandle->object;

    /* Try to create a sysvar */
    var = create_sysvar(key, NULL, type, flags.s_flags, buffer, len);

    if (!var)
        return HNDL_INVAL;

    /* We can put sysvars anywhere lol */
    if (KERR_ERR(sysvar_attach(target_object, var)))
        goto close_and_return_error;

    /* Inherit the target handles flags lol */
    init_khandle_ex(&new_khandle, target_khandle->flags, var->object);

    /* Try to bind the khandle */
    if (bind_khandle(&proc->m_handle_map, &new_khandle, (u32*)&ret))
        goto close_and_return_error;

    return ret;

close_and_return_error:
    oss_object_close(var->object);
    return HNDL_INVAL;
}
