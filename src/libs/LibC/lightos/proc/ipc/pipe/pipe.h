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
int init_lightos_pipe(lightos_pipe_t* pipe, const char* name, DWORD flags, DWORD max_listeners, DWORD datasize);
int init_lightos_pipe_uniform(lightos_pipe_t* pipe, const char* name, DWORD flags, DWORD max_listeners, DWORD datasize);

int destroy_lightos_pipe(lightos_pipe_t* pipe);

int lightos_pipe_dump(lightos_pipe_t* pipe, lightos_pipe_dump_t* pdump);

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
