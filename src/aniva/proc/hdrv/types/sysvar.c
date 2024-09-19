#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "proc/hdrv/driver.h"
#include "system/sysvar/map.h"
#include "system/sysvar/var.h"
#include <proc/env.h>
#include <proc/handle.h>
#include <proc/proc.h>
#include <sched/scheduler.h>
#include <system/profile/profile.h>

/*! */
static int sysvar_khdriver_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    sysvar_t* ret;
    proc_t* c_proc;
    penv_t* c_penv;
    user_profile_t* c_profile;

    ret = NULL;
    c_proc = get_current_proc();

    if (!c_proc)
        return -KERR_INVAL;

    c_penv = c_proc->m_env;

    /* Every process must have an environment */
    if (!c_penv)
        return -KERR_INVAL;

    c_profile = c_penv->profile;

    switch (mode) {
    case HNDL_MODE_CURRENT_ENV:
        ret = sysvar_get(c_penv->node, path);
        break;
    case HNDL_MODE_CURRENT_PROFILE:
        if (!c_profile)
            break;

        ret = sysvar_get(c_profile->node, path);
        break;
    default:
        break;
    }

    if (!ret)
        return -KERR_NOT_FOUND;

    /* Initialize the handle */
    init_khandle_ex(bHandle, driver->handle_type, flags, ret);

    return 0;
}

static int sysvar_khdriver_open_rel(khandle_driver_t* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    int error;
    sysvar_t* var;

    ASSERT_MSG(mode == HNDL_MODE_NORMAL, "TODO (sysvar_khdriver_open_rel): implement different handlemodes");

    /* Open the variable from the relative source */
    error = khandle_driver_generic_oss_open_from(driver, rel_hndl, path, flags, mode, bHandle);

    if (error)
        return error;

    /* Grab the thing */
    var = bHandle->reference.pvar;

    if (!var)
        return -KERR_INVAL;

    /* Reference it */
    get_sysvar(var);
    return 0;
}

static int sysvar_khdriver_read(khandle_driver_t* driver, khandle_t* handle, void* buffer, size_t bsize)
{
    sysvar_t* var;
    proc_t* c_proc;

    c_proc = get_current_proc();

    if (!c_proc)
        return -KERR_INVAL;

    /* Get the var */
    var = handle->reference.pvar;

    return sysvar_read(var, buffer, bsize);
}

static int sysvar_khdriver_write(khandle_driver_t* driver, khandle_t* handle, void* buffer, size_t bsize)
{
    kernel_panic("[sysvar] TODO: implement sysvar_khdriver_write");
}

khandle_driver_t sysvar_khdriver = {
    .name = "sysvar",
    .handle_type = HNDL_TYPE_SYSVAR,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL | KHDRIVER_FUNC_READ | KHDRIVER_FUNC_WRITE,
    .f_open = sysvar_khdriver_open,
    .f_open_relative = sysvar_khdriver_open_rel,
    .f_read = sysvar_khdriver_read,
    .f_write = sysvar_khdriver_write,
};
EXPORT_KHANDLE_DRIVER(sysvar_khdriver);
