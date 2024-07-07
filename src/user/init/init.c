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

    this_proc = open_proc("Runtime/User/init_env/init", HNDL_FLAG_R, NULL);

    if (!handle_verify(this_proc))
        return -43;

    error = lightos_pipe_connect(&pipe, this_proc, "test_pipe");

    if (error)
        return -55;

    /* Close the process handle */
    close_handle(this_proc);

    /* Keep polling for transactions */
    error = lightos_pipe_await_transaction(&pipe, &transaction);

    if (error)
        goto disconnect_and_return;

    /* Allocate our own buffer */
    buffer = malloc(transaction.data_size);

    /* Accept the incomming data */
    error = lightos_pipe_accept(&pipe, buffer, transaction.data_size);

    if (error)
        goto free_disconnect_and_return;

    printf("Recieved buffer: %s\n", buffer);

    /* Kill the temporary buffer */
free_disconnect_and_return:
    free(buffer);

disconnect_and_return:
    /* Disconnect from the pipe */
    lightos_pipe_disconnect(&pipe);
    return -45545;
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
    lightos_pipe_transaction_t test_transaction;
    char buffer[8] = { 0 };

    error = init_lightos_pipe(&pipe, "test_pipe", NULL, NULL);

    if (error)
        return error;

    create_process("Epic sauce", (FuncPtr)epic_sauce, NULL, NULL, NULL);

    buffer[0] = 'h';
    buffer[1] = 'e';
    buffer[2] = 'l';
    buffer[3] = 'l';
    buffer[4] = 'o';

    lightos_pipe_send(&pipe, &test_transaction, LIGHTOS_PIPE_TRANSACT_TYPE_DATA, buffer, sizeof(buffer));

    gets(buffer, sizeof(buffer) - 1);

    printf("Got: %s\n", buffer);

    destroy_lightos_pipe(&pipe);

    return 0;
}
