#include "lightos/handle_def.h"
#include "lightos/sysvar/shared.h"
#include "oss/node.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/sysvar/var.h"

enum HANDLE_TYPE sys_handle_get_type(HANDLE handle)
{
    khandle_t* c_handle;
    proc_t* c_proc;

    c_proc = get_current_proc();
    c_handle = find_khandle(&c_proc->m_handle_map, handle);

    if (!c_handle)
        return HNDL_TYPE_NONE;

    return c_handle->type;
}

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

    if (handle->type != HNDL_TYPE_SYSVAR)
        return SYSVAR_TYPE_INVAL;

    /* Extract the profile variable */
    var = handle->reference.pvar;

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
    khandle_t* khandle;
    enum PROFILE_TYPE target_prv_lvl;
    oss_node_t* target_node;

    proc = get_current_proc();

    if ((kmem_validate_ptr(proc, (uintptr_t)key, 1)))
        return false;

    if ((kmem_validate_ptr(proc, (uintptr_t)buffer, 1)))
        return false;

    khandle = find_khandle(&proc->m_handle_map, flags.s_rel_hndl);

    /* Invalid handle =/ */
    if (!khandle)
        return false;

    /* Can't write to this handle =/ */
    if ((khandle->flags & HNDL_FLAG_W) != HNDL_FLAG_W)
        return false;

    switch (khandle->type) {
    case HNDL_TYPE_PROFILE:
        target_node = khandle->reference.profile->node;
        target_prv_lvl = khandle->reference.profile->attr.ptype;
        break;
    case HNDL_TYPE_PROC_ENV:
        target_node = khandle->reference.penv->node;
        target_prv_lvl = khandle->reference.penv->attr.ptype;
        break;
    default:
        target_node = nullptr;
        break;
    }

    if (!target_node)
        return false;

    if (KERR_ERR(sysvar_attach(target_node, key, target_prv_lvl, type, flags.s_flags, (uintptr_t)buffer)))
        return false;

    return true;
}
