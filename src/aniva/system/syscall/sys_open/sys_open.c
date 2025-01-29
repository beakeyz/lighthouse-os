#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/error.h"
#include "lightos/syscall.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "oss/core.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <lightos/types.h>

static int oss_object_khdrv_open(const char* path, u32 flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode, khandle_t* bHandle)
{
    error_t error;
    oss_object_t* object;

    KLOG_INFO("Trying to open regular object %s of type %d\n", path, type);

    switch (mode) {
    case HNDL_MODE_NORMAL:
        error = oss_open_object(path, &object);

        /* Check if the type matches */
        if (IS_OK(error) && oss_object_valid_type(type) && object->type != type)
            error = -EBADTYPE;

        break;
    case HNDL_MODE_CREATE:
        /*  */
        error = oss_open_or_create_object(path, type, &object);

        if (EOK == error)
            break;

    case HNDL_MODE_CREATE_NEW:
        /* Create a new object for this process */
        object = create_oss_object(path, NULL, OT_GENERIC, oss_get_generic_ops(), NULL);

        if (!object)
            return -ENOMEM;

        /*
         * Need to take a reference, as this process will otherwise hold a loose
         * oss object
         */
        oss_object_ref(object);
        break;
    default:
        return -ENOIMPL;
    }

    if (error)
        return error;

    KLOG_INFO("Found object %s of type %d\n", object->key, object->type);

    /* Initialize this handle */
    init_khandle_ex(bHandle, flags, object);

    return 0;
}

static int oss_object_khdrv_open_relative(khandle_t* rel_hndl, const char* path, u32 flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode, khandle_t* bHandle)
{
    error_t error;
    oss_object_t* object;
    oss_object_t* rel_object;

    if (!rel_hndl)
        return -EINVAL;

    rel_object = rel_hndl->object;

    switch (mode) {
    case HNDL_MODE_NORMAL:
        error = oss_open_object_from(path, rel_object, &object);

        /* Check if the type matches */
        if (IS_OK(error) && oss_object_valid_type(type) && object->type != type)
            error = -EBADTYPE;
        break;
    case HNDL_MODE_CREATE:
        error = oss_open_or_create_object_from(path, type, rel_object, &object);

        if (IS_OK(error))
            break;

    case HNDL_MODE_CREATE_NEW:
        error = EOK;
        /* Try to create a new object for the user */
        object = create_oss_object(path, NULL, OT_GENERIC, oss_get_generic_ops(), NULL);

        if (!object)
            error = -ENOMEM;

        break;
    default:
        return -ENOIMPL;
    }

    if (error)
        return error;

    /* Initialize this handle */
    init_khandle_ex(bHandle, flags, object);

    return 0;
}

/*
 * Generic open syscall
 *
 * TODO: Refactor
 * We can make this much cleaner. Every handle type can have it's own sys_open implementation, which
 * we just have to look up. This way we don't need this giant ugly switch statement
 */
HANDLE sys_open(const char* path, handle_flags_t flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode)
{
    HANDLE ret;
    khandle_t* rel_khndl;
    khandle_t handle = { 0 };
    kerror_t error;
    proc_t* c_proc;

    KLOG_INFO("Trying to open %s\n", path);

    c_proc = get_current_proc();

    if (!c_proc || !path || (kmem_validate_ptr(c_proc, (uintptr_t)path, 1)))
        return HNDL_INVAL;

    if (HNDL_IS_VALID(flags.s_rel_hndl)) {
        rel_khndl = find_khandle(&c_proc->m_handle_map, flags.s_rel_hndl);

        /* Invalid relative handle anyway =( */
        if (!rel_khndl)
            return HNDL_INVAL;

        if (oss_object_khdrv_open_relative(rel_khndl, path, flags.s_flags, type, mode, &handle))
            return HNDL_NOT_FOUND;
    } else
        /* Just mark it as 'not found' if the driver fails to open */
        if (oss_object_khdrv_open(path, flags.s_flags, type, mode, &handle))
            return HNDL_NOT_FOUND;

    /*
     * TODO: check for permissions and open with the appropriate flags
     *
     * We want to do this by checking the profile that this process is in and checking if processes
     * in that profile have access to this resource
     */

    /* Check if the handle was really initialized */
    if (!handle.object)
        return HNDL_NOT_FOUND;

    /* Copy the handle into the map */
    error = bind_khandle(&c_proc->m_handle_map, &handle, (uint32_t*)&ret);

    if (error)
        ret = HNDL_NOT_FOUND;

    return ret;
}

/*!
 * @brief: Read from a directory at index @idx
 */
HANDLE sys_open_idx(handle_t rel, uint32_t idx, handle_flags_t flags)
{
    /*
     * TODO: We need to support index-based opening natively on oss objects. This is quite easy
     * as we can simply do the reverse of a regular oss_object_open, where we check existing connections
     * and then query the object if we cant find a particular connection.
     * For index-based opening, it's imperative that we FIRST probe the object and then look at
     * the connections. The problem is that connections don't have a predetermined order, so shit might
     * just hit the fan. We need to follow this order, because if the object we're accessing is a filesystem-backed
     * object, the filesystem order of objects should be respected and given precedence over the order of connections.
     *
     * We need to think about this carefully, because it might happen that a fsroot object has connections to other
     * oss object, other than the ones it owns by extention of the filesystem. How are we going to correctly index all
     * these object, without potentially counting some double?
     */
    HANDLE ret;
    proc_t* c_proc;
    khandle_t* khandle;
    khandle_t new_handle;
    oss_object_t* new_object;

    c_proc = get_current_proc();

    khandle = find_khandle(&c_proc->m_handle_map, rel);

    if (!khandle)
        return HNDL_INVAL;

    /* Try to find an object on this guy */
    if (oss_open_object_from_idx(idx, khandle->object, &new_object))
        return HNDL_INVAL;

    /* First initialize the new handle */
    init_khandle_ex(&new_handle, flags.s_flags, new_object);

    /* Bind the handle */
    bind_khandle(&c_proc->m_handle_map, &new_handle, (u32*)&ret);

    return ret;
}

static inline enum OSS_CONNECTION_DIRECTION __get_oss_direction_from_flags(handle_flags_t flags)
{
    if ((flags.s_flags & HF_U) == HF_U)
        return OSS_CONNECTION_UPSTREAM;

    if ((flags.s_flags & HF_D) == HF_D)
        return OSS_CONNECTION_DOWNSTREAM;

    return OSS_CONNECTION_DONTCARE;
}

HANDLE sys_open_connected_idx(handle_t rel, uint32_t idx, handle_flags_t flags)
{
    HANDLE ret;
    proc_t* c_proc;
    khandle_t* khandle;
    khandle_t new_handle;
    oss_object_t* new_object;
    enum OSS_CONNECTION_DIRECTION direction;

    c_proc = get_current_proc();

    khandle = find_khandle(&c_proc->m_handle_map, rel);

    if (!khandle)
        return HNDL_INVAL;

    /* Get the desired connection direction */
    direction = __get_oss_direction_from_flags(flags);

    /* Try to find an object on this guy */
    if (oss_open_connected_object_from_idx(idx, khandle->object, &new_object, direction))
        return HNDL_INVAL;

    /* First initialize the new handle */
    init_khandle_ex(&new_handle, flags.s_flags, new_object);

    /* Bind the handle */
    bind_khandle(&c_proc->m_handle_map, &new_handle, (u32*)&ret);

    return ret;
}

void* sys_get_function(HANDLE lib_handle, const char __user* path)
{
    proc_t* c_proc;

    c_proc = get_current_proc();

    if (kmem_validate_ptr(c_proc, (vaddr_t)path, 1))
        return NULL;

    /* NOTE: Dangerous */
    if (!strlen(path))
        return NULL;

    kernel_panic("TODO: sys_get_function");
}

error_t sys_close(HANDLE handle)
{
    kerror_t error;
    proc_t* current_process;

    current_process = get_current_proc();

    /* Destroying the khandle */
    error = unbind_khandle(&current_process->m_handle_map, handle);

    if (error)
        return EINVAL;

    return 0;
}
