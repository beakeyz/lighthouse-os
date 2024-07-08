#include "lightos/proc/ipc/pipe/pipe.h"
#include "lightos/proc/ipc/pipe/shared.h"
#include "lightos/proc/process.h"
#include "time.h"
#include "unistd.h"
#include <lightos/fs/dir.h>
#include <lightos/memory/alloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

int epic_sauce()
{
    int error;
    int idx = 0;
    char alphabet[27] = { 0 };
    lightos_pipe_t pipe;

    printf("Epic sauce\n");

    error = lightos_pipe_connect(&pipe, "Runtime/Admin/init_env/test_pipe");

    if (error)
        return error;

    do {
        char buffer[2] = { 0 };

        if (idx % 2 == 0)
            /* Accept the incomming data */
            error = lightos_pipe_accept(&pipe, buffer, sizeof(buffer));
        else
            error = lightos_pipe_deny(&pipe);

        if (error)
            continue;

        alphabet[idx] = (idx % 2 == 0) ? buffer[0] : ' ';
        idx++;
    } while (idx < 26);

    printf("Recieved the alphabet: %s\n", alphabet);

    /* Disconnect from the pipe */
    lightos_pipe_disconnect(&pipe);
    return error;
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
    int error;
    lightos_pipe_t pipe;
    lightos_pipe_dump_t dump;
    lightos_pipe_transaction_t test_transaction;
    char buffer[2] = { 0 };

    error = init_lightos_pipe(&pipe, "test_pipe", LIGHTOS_UPIPE_FLAGS_FULLDUPLEX, NULL, NULL);

    if (error)
        return error;

    create_process("Epic sauce", (FuncPtr)epic_sauce, NULL, NULL, NULL);

    buffer[0] = 'a';

    /* Send the alphabet */
    for (int i = 0; i < 26; i++) {
        lightos_pipe_send(&pipe, &test_transaction, LIGHTOS_PIPE_TRANSACT_TYPE_DATA, buffer, sizeof(buffer));

        buffer[0]++;
    }

    printf("Waiting\n");

    /* Wait until all our transactions have been handled */
    do {
        error = lightos_pipe_dump(&pipe, &dump);

        if (error)
            break;

        usleep(1000);
    } while (lightos_pipe_n_total_transact(&dump) < 26);

    printf("Done! transaction rate: %d%%\n", dump.acceptance_rate);

    destroy_lightos_pipe(&pipe);

    return 0;
}
