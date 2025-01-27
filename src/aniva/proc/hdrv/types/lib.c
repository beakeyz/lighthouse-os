#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "proc/exec/exec.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include "sched/scheduler.h"
#include <dev/driver.h>

/*
 * TODO:
 */

static int __lib_khdrv_open(struct khandle_driver* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    proc_t* c_proc;
    process_library_t* lib;
    aniva_exec_method_t* method;

    if (!path)
        return -KERR_INVAL;

    c_proc = get_current_proc();

    /* Try to get the method used to launch this process */
    method = aniva_exec_method_get(c_proc->exec_key);

    if (!method || !method->f_get_lib)
        return -KERR_NOT_FOUND;

    /* Get the library from the execution method */
    lib = method->f_get_lib(c_proc, path);

    /* Couldn't find shit =( */
    if (!lib)
        return -ENOENT;

    /* Initialize the handle */
    init_khandle_ex(bHandle, driver->handle_type, flags, lib);

    return 0;
}

khandle_driver_t lib_khandle_drv = {
    .name = "SharedLib",
    .handle_type = HNDL_TYPE_SHARED_LIB,
    .function_flags = KHDRIVER_FUNC_OPEN,
    .f_open = __lib_khdrv_open,
};
EXPORT_KHANDLE_DRIVER(lib_khandle_drv);
