#include "system/profile/profile.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "oss/node.h"
#include "proc/handle.h"
#include <proc/hdrv/driver.h>

static int profile_khdriver_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    int error;
    user_profile_t* profile;

    switch (mode) {
    case HNDL_MODE_NORMAL:
        /* Try to find the profile relative to the runtime node */
        error = profile_find(path, &profile);
        break;
    case HNDL_MODE_CURRENT_PROFILE:
        profile = get_active_profile();
        error = KERR_NONE;
        break;
    case HNDL_MODE_CREATE:
        error = profile_find(path, &profile);

        if (error) {
            profile = create_proc_profile((char*)path, HNDL_FLAGS_GET_PERM(flags));

            /* Reset the error var */
            error = profile != nullptr ? 0 : -KERR_INVAL;
        }

        break;
    case HNDL_MODE_CREATE_NEW:
        error = profile_find(path, &profile);

        /* If this already exists, we're fucked */
        if (KERR_OK(error)) {
            error = -KERR_DUPLICATE;
            break;
        }

        profile = create_proc_profile((char*)path, HNDL_FLAGS_GET_PERM(flags));

        /* Reset the error var */
        error = profile != nullptr ? 0 : -KERR_INVAL;

        break;
    default:
        error = -KERR_INVAL;
        break;
    }

    if (error)
        return error;

    /* Initialize the khandle */
    init_khandle_ex(bHandle, driver->handle_type, flags, profile);

    return 0;
}

static int profile_khdriver_open_from(khandle_driver_t* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    int error;
    oss_node_t* rel_node;
    user_profile_t* profile;

    rel_node = khandle_get_relative_node(rel_hndl);

    if (!rel_node)
        return -KERR_INVAL;

    error = profile_find_from(rel_node, path, &profile);

    if (error)
        return error;

    /* Initialize the khandle */
    init_khandle_ex(bHandle, driver->handle_type, flags, profile);

    return 0;
}

khandle_driver_t profile_khdriver = {
    .name = "profiles",
    .handle_type = HNDL_TYPE_PROFILE,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL,
    .f_open = profile_khdriver_open,
    .f_open_relative = profile_khdriver_open_from,
};
EXPORT_KHANDLE_DRIVER(profile_khdriver);
