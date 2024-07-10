#include "lightos/proc/process.h"
#include "time.h"
#include <lightos/fs/dir.h>
#include <lightos/memory/alloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

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
    create_process("Root/Apps/doom -iwad Root/Apps/doom1.wad", NULL, NULL, NULL, NULL);

    return 0;
}
