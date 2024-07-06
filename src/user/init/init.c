#include "lightos/handle.h"
#include "lightos/proc/ipc/pipe/pipe.h"
#include "lightos/proc/ipc/pipe/shared.h"
#include "lightos/proc/process.h"
#include <lightos/fs/dir.h>
#include <lightos/memory/alloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int epic_sauce()
{
    int error;
    HANDLE this_proc;
    char* buffer;
    lightos_pipe_t pipe;
    lightos_pipe_transaction_t transaction;

    printf("Epic sauce\n");
    return 0;
}

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
    lightos_pipe_t pipe;
    lightos_pipe_transaction_t test_transaction;
    char buffer[8];

    // init_lightos_pipe(&pipe, "test_pipe");

    create_process("Epic sauce", (FuncPtr)epic_sauce, NULL, NULL, NULL);

    printf("Input: ");
    gets(buffer, sizeof(buffer));

    // lightos_pipe_send(&pipe, &test_transaction, LIGHTOS_PIPE_TRANSACT_TYPE_DATA, buffer, sizeof(buffer));

    printf("Got: %s\n", buffer);
    return 0;
}
