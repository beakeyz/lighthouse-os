#ifndef __ANIVA_THR_INTRF__
#define __ANIVA_THR_INTRF__
#include <libk/stddef.h>
#include <libk/flow/error.h>
#include "proc/thread.h"

/*
 * TODO: let's actually figure out how we want to approach this problem...
 */

// some handy structs
struct thread;
struct threaded_socket;
struct tspckt;

typedef struct thread_interface {
} thread_interface_t;

/*
 * When the current thread is compatible with the ANIVA thread interface model (i.e. sockets, drivers, etc.)
 * we give it access to the interface that is mapped to this port
 */
ErrorOrPtr request_thread_if(uint32_t port);

ErrorOrPtr attach_interface_to_thread(thread_t* thread);
ErrorOrPtr detach_interface_from_thread(thread_t* thread);

/*
 * "Map" a given interface to a port
 * TODO: lookup table for this or something else?
 */
ErrorOrPtr map_interface(uint32_t port);
ErrorOrPtr unmap_interface(uint32_t port);

ErrorOrPtr wait_for_if_signal();

#endif //__ANIVA_THR_INTRF__
