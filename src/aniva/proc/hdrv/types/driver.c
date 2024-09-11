#include "dev/core.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"
#include <dev/driver.h>
#include <proc/env.h>
#include <proc/hdrv/driver.h>

/**
 * @brief: Send a read command to a driver
 *
 * This tries to locate the drives control device object.
 */
static int driver_khdriver_read(khandle_driver_t* driver, khandle_t* khndl, void* buffer, size_t bsize)
{
    kerror_t error;
    driver_t* drv;

    /* Grab the driver from the handle */
    drv = khndl->reference.driver;

    error = drv_read(drv, buffer, &bsize, khndl->offset);

    /* Oof */
    khndl->offset += bsize;

    return error;
}

static int driver_khdriver_write(khandle_driver_t* driver, khandle_t* khndl, void* buffer, size_t bsize)
{
    kerror_t error;
    driver_t* drv;

    /* Grab the driver from the handle */
    drv = khndl->reference.driver;

    error = drv_write(drv, buffer, &bsize, khndl->offset);

    /* Oof */
    khndl->offset += bsize;

    return error;
}

static int driver_khdriver_ctl(khandle_driver_t* driver, khandle_t* khndl, dcc_t code, u64 offset, void* buffer, size_t bsize)
{
    driver_t* drv;

    /* Grab the driver from the handle */
    drv = khndl->reference.driver;

    return driver_send_msg_ex(drv, code, buffer, bsize, buffer, bsize);
}

static int driver_khdriver_destroy(khandle_driver_t* driver, khandle_t* handle)
{
    driver_t* drv;
    proc_t* c_proc;

    drv = handle->reference.driver;

    if (!drv)
        return -KERR_INVAL;

    /* Get the current process */
    c_proc = get_current_proc();

    if (!c_proc)
        return -KERR_INVAL;

    /* Only admin processes may destroy drivers */
    if (c_proc->m_env->priv_level != PRIV_LVL_ADMIN)
        return -KERR_NOPERM;

    if (is_driver_loaded(drv))
        /* If the driver is still loaded, simply unload it */
        return unload_driver(drv->m_url);

    /* If this driver was already unloaded, we uninstall it */
    return uninstall_driver(drv);
}

static khandle_driver_t driver_khdriver = {
    .name = "drivers",
    .handle_type = HNDL_TYPE_DRIVER,
    .function_flags = KHDRIVER_FUNC_ALL,
    .f_open = khandle_driver_generic_oss_open,
    .f_open_relative = khandle_driver_generic_oss_open_from,
    .f_read = driver_khdriver_read,
    .f_write = driver_khdriver_write,
    .f_ctl = driver_khdriver_ctl,
    .f_destroy = driver_khdriver_destroy,
};
EXPORT_KHANDLE_DRIVER(driver_khdriver);
