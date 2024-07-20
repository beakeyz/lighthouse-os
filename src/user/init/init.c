#include "params/params.h"
#include <errno.h>
#include <lightos/driver/drv.h>
#include <lightos/fs/dir.h>
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
/*
 * What should the init process do?
 *  - Verify system integrity before bootstrapping further
 *  - Find various config files and load the ones it needs
 *  - Try to configure the kernel trough the essensial configs
 *  - Find the vector of services to run, like in no particular order
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
    CMDLINE cmd;

    if (cmdline_get(&cmd))
        return -EINVAL;

    params_parse((const char**)cmd.argv, cmd.argc, params, (sizeof params / sizeof params[0]));

    /* Load the pipes driver if it's loading is prevented */
    if (!no_pipes)
        load_driver("Root/System/upi.drv", NULL, NULL);

    return (success) ? 0 : -EINVAL;
}
