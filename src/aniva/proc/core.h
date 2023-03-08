#ifndef __ANIVA_PROC_CORE__
#define __ANIVA_PROC_CORE__

#include "libk/async_ptr.h"
#include "libk/linkedlist.h"
#include "libk/error.h"
#include "libk/vector.h"
#include "libk/queue.h"

#define DEFAULT_STACK_SIZE (16 * Kib)
#define DEFAULT_THREAD_MAX_TICKS 4

#define PROC_DEFAULT_MAX_THREADS 16
#define PROC_CORE_PROCESS_NAME "[aniva-core]"

#define MIN_SOCKET_BUFFER_SIZE 0
// FIXME: should this be the max size?
#define SOCKET_DEFAULT_SOCKET_BUFFER_SIZE 4096
#define SOCKET_DEFAULT_MAXIMUM_SOCKET_COUNT 128
#define SOCKET_DEFAULT_MAXIMUM_BUFFER_COUNT 64

struct tspckt;
struct threaded_socket;
struct packet_response;

typedef enum thread_state {
  INVALID = 0,      // not initialized
  RUNNING,          // executing
  RUNNABLE,         // can be executed by the scheduler
  DYING,            // waiting to be cleaned up
  DEAD,             // thread is destroyed, the scheduler can remove it from the pool
  STOPPED,          // stopped by the scheduler, for whatever reason. waiting for reschedule
  BLOCKED,          // performing blocking operation
  SLEEPING,         // waiting for anything to happen (i.e. signals, data)
  NO_CONTEXT,       // waiting to recieve a context to run
} thread_state_t;

ANIVA_STATUS initialize_proc_core();

/*
 * Register a socket on the kernel socket chain
 */
ErrorOrPtr socket_register(struct threaded_socket* socket);

/*
 * unregister a socket from the kernel socket chain
 */
ErrorOrPtr socket_unregister(struct threaded_socket* socket);

/*
 * Try to grab a new proc_id
 */
ErrorOrPtr generate_new_proc_id();

/*
 * return a pointer to the socket register
 */
list_t get_registered_sockets();

/*
 * find a socket based on its port
 * TODO: validate port based on checksum?
 */
struct threaded_socket *find_registered_socket(uint32_t port);

/*
 * send a data-packet to a port
 * returns a pointer to the response ptr (thus a double pointer)
 * if all goes well, otherwise a nullptr
 */
extern async_ptr_t* send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size); // socket.c

/*
 * above function but blocking
 */
extern struct packet_response*send_packet_to_socket_blocking(uint32_t port, void* buffer, size_t buffer_size); // socket.c

/*
 * validata a tspckt based on its identifier (checksum, hash, idk man)
 */
extern bool validate_tspckt(struct tspckt* packet); // tspctk.c


#endif //__LIGHTHOUSE_OS_CORE__
