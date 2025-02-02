#ifndef __LIGTOS_LIBC_PIPE__
#define __LIGTOS_LIBC_PIPE__

#include "shared.h"
#include <lightos/handle.h>
#include <stdint.h>

/*!
 * @brief: Initialize a new pipe struct
 *
 * Creates an oss object inside the processes environment
 * to represent an endpoint for other processes to connect to
 *
 * @pipe: The pipe struct to initialize
 */
int init_lightos_pipe(lightos_pipe_t* pipe, const char* name, u32 flags, u32 max_listeners, u32 datasize);

/*!
 * @brief: Initialize a pipe for uniform data
 *
 * Wrapper for init_lightos_pipe
 *
 */
int init_lightos_pipe_uniform(lightos_pipe_t* pipe, const char* name, u32 flags, u32 max_listeners, u32 datasize);
int init_lightos_pipe_global(lightos_pipe_t* pipe, const char* name, u32 flags, u32 max_listeners, u32 datasize);

/*!
 * @brief: Destroys a lightos pipe object
 *
 * Instantly ends pipe lifetime. This should pretty much always be called when a host process
 * is ending, since pipes most of the time live inside a processes environment, which gets killed
 * together with the process itself.
 */
int destroy_lightos_pipe(lightos_pipe_t* pipe);

int lightos_pipe_dump(lightos_pipe_t* pipe, lightos_pipe_dump_t* pdump);

int lightos_pipe_is_empty(lightos_pipe_t* pipe, BOOL* empty);
int lightos_pipe_await_empty(lightos_pipe_t* pipe);

/*!
 * @brief: Connect to an already existing pipe
 *
 */
int lightos_pipe_connect(lightos_pipe_t* pipe, const char* path);
int lightos_pipe_connect_rel(lightos_pipe_t* pipe, HANDLE rel_handle, const char* name);
int lightos_pipe_disconnect(lightos_pipe_t* pipe);

int lightos_pipe_send(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, int type, void* pdata, size_t size);
int lightos_pipe_send_data(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, void* pdata, size_t size);
int lightos_pipe_send_signal(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, int signal);
int lightos_pipe_send_handle(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction, HANDLE handle);

int lightos_pipe_await_transaction(lightos_pipe_t* pipe, lightos_pipe_transaction_t* transaction);
int lightos_pipe_preview(lightos_pipe_t* pipe, lightos_pipe_transaction_t* p_transaction);
int lightos_pipe_accept(lightos_pipe_t* pipe, void* pdata, size_t psize);
int lightos_pipe_deny(lightos_pipe_t* pipe);

#endif // !__LIGTOS_LIBC_PIPE__
