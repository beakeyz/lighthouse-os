#include "proc/env.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "oss/core.h"
#include "oss/node.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"

static int __penv_hdriver_open_rel(khandle_driver_t* driver, oss_node_t* rel_node, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    oss_node_t* node = nullptr;
    penv_t* env;

    /*
     * Try to resolve a node at this path
     * TODO: Check for node permissions
     */
    if (oss_resolve_node_rel(rel_node, path, &node))
        return -KERR_NOT_FOUND;

    if (node->type != OSS_PROC_ENV_NODE)
        return -KERR_NOT_FOUND;

    /* Try to grab the 'private' field of the node, which should contain our env */
    env = oss_node_unwrap(node);

    if (!env)
        return -KERR_NOT_FOUND;

    init_khandle_ex(bHandle, driver->handle_type, flags, env);

    return 0;
}

static int penv_hdriver_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    return __penv_hdriver_open_rel(driver, NULL, path, flags, mode, bHandle);
}

static int penv_hdriver_open_rel(khandle_driver_t* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    oss_node_t* rel_node;

    /* Try to get the relative node */
    rel_node = khandle_get_relative_node(rel_hndl);

    /* Call the sub routine */
    return __penv_hdriver_open_rel(driver, rel_node, path, flags, mode, bHandle);
}

khandle_driver_t penv_hdriver = {
    .name = "penv",
    .handle_type = HNDL_TYPE_PROC_ENV,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL,
    .f_open = penv_hdriver_open,
    .f_open_relative = penv_hdriver_open_rel,
};
EXPORT_KHANDLE_DRIVER(penv_hdriver);
