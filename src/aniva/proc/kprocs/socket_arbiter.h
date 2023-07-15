#ifndef __ANIVA_SOCKET_ARBITER__
#define __ANIVA_SOCKET_ARBITER__

#include "libk/flow/error.h"
#include "proc/proc.h"
#include "proc/socket.h"

#define SOCK_ARB_FLAG_YIELD_AFTER_HANDLE    (0x00000001)
#define SOCK_ARB_FLAG_DISABLE               (0x00000002)

void init_socket_arbiter(proc_t* process);

ErrorOrPtr socket_arbiter_register_socket(threaded_socket_t* socket);
ErrorOrPtr socket_arbiter_remove_socket(threaded_socket_t* socket);

#endif // !__ANIVA_SOCKET_ARBITER__
