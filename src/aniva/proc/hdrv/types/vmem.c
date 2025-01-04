#include "lightos/handle_def.h"
#include "lightos/memory/memory.h"
#include "lightos/sysvar/shared.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "oss/path.h"
#include "proc/env.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sys/types.h"
#include "system/profile/profile.h"
#include "system/sysvar/map.h"
#include "system/sysvar/var.h"

static sysvar_t* __get_sysvar(penv_t* env, const char* path)
{
    sysvar_t* ret;

    /* First try the proc environment */
    error_t error = penv_get_var(env, path, &ret);

    if (!error && ret)
        return ret;

    /* Shit. Try the profile */
    error = profile_get_var(env->profile, path, &ret);

    if (!error && ret)
        return ret;

    /* FUCKKKK try all other profiles */
    error = profile_find_var(path, &ret);

    /* Fucking invalid path. SHiiiit */
    if (error)
        return nullptr;

    return ret;
}


/*!
 * @brief: Basic virtual memory mapping open function
 */
int vmem_open(struct khandle_driver* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    error_t error;
    proc_t* c_proc;
    penv_t* c_env;
    sysvar_t* var;
    /* Create an oss path to parse @path */
    oss_path_t oss_path;

    if (!path || !bHandle)
        return -EINVAL;

    /* Grab the current process */
    c_proc = get_current_proc();

    if (!c_proc)
        return -EINVAL;

    /* Grab this processes environment */
    c_env = c_proc->m_env;

    /* Every process must have an environment */
    if (!c_env)
        return -EINVAL;

    /* Initialize the error */
    error = EOK;

    /* Check what we need to do */
    switch (mode) {
        case HNDL_MODE_NORMAL:
            var = __get_sysvar(c_env, path);
            break;
        case HNDL_MODE_CREATE:
            var = __get_sysvar(c_env, path);

            /* If the variable already exists, we're guccie */
            if (var)
                break;

            // Fallthrough
        case HNDL_MODE_CREATE_NEW:
            /* Parse this path */
            error = oss_parse_path(path, &oss_path);

            /* Invalid path? */
            if (error)
                break;

            KLOG_DBG("Path: %s, thing: %s\n", path, oss_path.subpath_vec[oss_path.n_subpath-1]);

            /* Create a new sysvar if it didnt exists yet */
            var = create_sysvar(oss_path.subpath_vec[oss_path.n_subpath-1], c_env->profile->attr.ptype, SYSVAR_TYPE_MEM_RANGE, NULL, NULL, NULL);

            /* Kill the path */
            oss_destroy_path(&oss_path);

            /* Attach the variable to the node */
            error = sysvar_attach(c_env->node, var);
            break;
        default:
            return -ENOTSUP;
    }

    if (!var)
        return -ENOENT;

    if (error)
        return error;

    /* Initialize the handle */
    init_khandle_ex(bHandle, driver->handle_type, flags, var);

    return error;
}

int vmem_open_rel(struct khandle_driver* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    return 0;
}

/*!
 * @brief: Close a handle to vmem
 *
 * Since a vmem handle is a special handle to a sysvar, just close that shit
 */
int vmem_close(struct khandle_driver* driver, khandle_t* handle)
{
    error_t error;
    sysvar_t* var;
    page_range_t range;

    /* Verify the typ */
    if (handle->type != HNDL_TYPE_VMEM)
        return -EINVAL;

    var = handle->reference.vmem_range;

    if (!var)
        return -KERR_INVAL;

    sysvar_lock(var);

    error = sysvar_read(var, &range, sizeof(range));

    /* Can't read? */
    if (error)
        goto unlock_and_release;

    /* Check if there are actual ranges allocated */
    if (!range.nr_pages)
        goto unlock_and_release;

    /* Dellocate the range from the users addresspace */
    error = kmem_user_dealloc(get_current_proc(), kmem_get_page_addr(range.page_idx), range.nr_pages << PAGE_SHIFT);

unlock_and_release:
    sysvar_unlock(var);

    release_sysvar(var);
    return error;
}

khandle_driver_t vmem_driver = {
    .name = "vmem",
    .handle_type = HNDL_TYPE_VMEM,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL | KHDRIVER_FUNC_CLOSE,
    .f_open = vmem_open,
    .f_open_relative = vmem_open_rel,
    .f_close = vmem_close,
};
EXPORT_KHANDLE_DRIVER(vmem_driver);
