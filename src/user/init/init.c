#include "params/params.h"
#include <errno.h>
#include <lightos/fs/dir.h>
#include <lightos/memory/alloc.h>
#include <lightos/proc/cmdline.h>
#include <stdio.h>
#include <sys/types.h>

BOOL test;
char buffer[8];
uint32_t num;

cmd_param_t params[] = {
    CMD_PARAM_AUTO(test, CMD_PARAM_TYPE_BOOL, 't', "Does a test"),
    CMD_PARAM_AUTO_2_ALIAS(buffer, CMD_PARAM_TYPE_STRING, 'b', "Does a buffer", "set-buffer", "do-thing"),
    CMD_PARAM_AUTO(num, CMD_PARAM_TYPE_NUM, 'n', "Does a number"),
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
    CMDLINE cmd;
    //(void)create_process("Root/Apps/doom -iwad Root/Apps/doom1.wad", NULL, NULL, NULL, NULL);

    if (cmdline_get(&cmd))
        return -EINVAL;

    params_parse((const char**)cmd.argv, cmd.argc, params, 3);

    printf("Test was set to: %s, (%d)\n", test ? "true" : "false", test);
    buffer[7] = '\0';
    printf("Buffer was set to: %s\n", buffer);
    printf("Num was set to: %d\n", num);
    return 0;
}
