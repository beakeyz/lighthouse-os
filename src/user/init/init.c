#include "devices/device.h"
#include "devices/shared.h"
#include "lightos/fs/shared.h"
#include "lightos/handle_def.h"
#include "params/params.h"
#include <errno.h>
#include <lightos/driver/drv.h>
#include <lightos/fs/dir.h>
#include <lightos/handle.h>
#include <lightos/memory/alloc.h>
#include <lightos/proc/cmdline.h>
#include <lightos/proc/process.h>
#include <sys/types.h>

BOOL use_kterm = false;
BOOL no_pipes = false;

cmd_param_t params[] = {
    CMD_PARAM_AUTO_1_ALIAS(use_kterm, CMD_PARAM_TYPE_BOOL, 'k', "Use kterm", "use-kterm"),
    CMD_PARAM_AUTO_1_ALIAS(no_pipes, CMD_PARAM_TYPE_BOOL, 'p', "Don't load the pipes driver", "no-pipes"),
};

/*!
 * @brief: Checks if there are any funnie hid device
 *
 * For the system to function, we kinda need a keyboard and
 * a mouse would also be nice lmao
 */
static void check_hid_devices(bool* p_haskeyboard, bool* p_hasmouse)
{
    DEVINFO binfo;
    HANDLE dev_handle;
    DirEntry* c_entry;
    Directory* dir;

    if (!p_hasmouse || !p_haskeyboard)
        return;

    /* Set both to false, as a default */
    *p_haskeyboard = false;
    *p_hasmouse = false;

    dir = open_dir("Dev/hid", HNDL_FLAG_R, HNDL_MODE_NORMAL);

    if (!dir)
        return;

    for (int i = 0;; i++) {
        c_entry = dir_read_entry(dir, i);

        if (!c_entry)
            break;

        /* Not a device, skip */
        if (c_entry->entry.type != LIGHTOS_DIRENT_TYPE_DEV)
            continue;

        dev_handle = dir_entry_open(dir, c_entry, HNDL_FLAG_R, HNDL_MODE_NORMAL);

        /* Not a valid handle for some reason xD */
        if (!handle_verify(dev_handle))
            continue;

        /* Can't seem to query this device, go next */
        if (!device_query_info(dev_handle, &binfo))
            goto cycle;

    cycle:
        close_handle(dev_handle);
    }

    close_dir(dir);
    return;
}

/*!
 * @brief: Perform runtime system initialisation from the user-side
 *
 * This process is sent into userland by the kernel to do things there on its behalf xD
 * The kernel has already done some basic device detection and driver matching, but it might
 * have fucked up at some fronts. This process further loads some device/utility drivers and
 * does some more scanning.
 *
 * What should the init process do?
 *  - Verify system integrity before bootstrapping further
 *  - Find various config files and load the ones it needs
 *     * Load and read some sysvar files which may contain driver lists we need to load \o/
 *  - Try to configure the kernel trough the essensial configs
 *  - Find the vector of services to run like, in no particular order:
 *      -- Display manager
 *      -- Sessions
 *      -- Network services
 *      -- Peripherals
 *      -- Audio
 *  - Find out if we need to install the system on a permanent storage device and
 *    if so, run the installer (which creates the partition, installs the fs and the system)
 *  - Find the vector of further bootstrap applications to run and run them
 */
int main()
{
    BOOL success = true;
    BOOL has_kb, has_mouse;
    CMDLINE cmd;

    if (cmdline_get(&cmd))
        return -EINVAL;

    params_parse((const char**)cmd.argv, cmd.argc, params, (sizeof params / sizeof params[0]));

    /* Check for essential HID devices */
    check_hid_devices(&has_kb, &has_mouse);

    /* Load the fallback PS2 emulation driver when we are poor */
    if (!has_kb && !has_mouse)
        load_driver("Root/System/i8042.drv", NULL, NULL);

    /* Load the pipes driver if it's loading is prevented */
    if (!no_pipes)
        load_driver("Root/System/upi.drv", NULL, NULL);

    return (success) ? 0 : -EINVAL;
}
