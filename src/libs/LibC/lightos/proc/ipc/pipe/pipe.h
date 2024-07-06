#ifndef __LIGTOS_LIBC_PIPE__
#define __LIGTOS_LIBC_PIPE__

#include "shared.h"
#include <lightos/handle.h>
#include <stdint.h>

/*
 * Multiduplex IPC struct
 *
 * Processes can send transactions over the pipe and the other processes on the pipe
 * can choose to accept or deny the transactions.
 *
 * Pipes are represented in the OSS by objects inside processes environments. These pipes
 * are managed by the other/upi (user pipe) driver, which needs to be loaded in order to have
 * communication
 *
 * Communication through pipes
 */
typedef struct lightos_pipe {
    /* Handle to the pipe object */
    HANDLE pipe;
    /* The mode this pipe is in */
    enum LIGHTOS_PIPE_MODE mode;
    /* Pipes should have short names */
    const char name[32];
} lightos_pipe_t;

/*!
 * @brief: Initialize a new pipe struct
 *
 * Creates an oss object inside the processes environment
 * to represent an endpoint for other processes to connect to
 *
 * @pipe: The pipe struct to initialize
 */
int init_lightos_pipe(lightos_pipe_t* pipe, const char* name);

/*!
 * @brief: Connect to an already existing pipe
 *
 */
int lightos_pipe_connect(lightos_pipe_t* pipe, HANDLE process, const char* name);
int lightos_pipe_disconnect(lightos_pipe_t* pipe);

int lightos_pipe_send(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, int type, void* pdata, size_t size);

int lightos_pipe_preview(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction);
int lightos_pipe_accept(lightos_pipe_t* pipe, void* pdata, size_t psize);
int lightos_pipe_deny(lightos_pipe_t* pipe);

#endif // !__LIGTOS_LIBC_PIPE__
