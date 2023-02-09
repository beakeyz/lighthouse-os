#ifndef __ANIVA_PROC_CORE__
#define __ANIVA_PROC_CORE__

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

struct threaded_socket;

typedef enum thread_state {
  INVALID = 0,
  RUNNING,
  RUNNABLE,
  DYING,
  DEAD,
  STOPPED,
  BLOCKED,
  NO_CONTEXT,
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
 * find a socket based on its port
 * TODO: validate port based on checksum?
 */
struct threaded_socket *find_registered_socket(uint32_t port);

/*
 * send a data-packet to a port
 * returns a pointer to the response ptr (thus a double pointer)
 * if all goes well, otherwise a nullptr
 */
extern ErrorOrPtr send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size); // socket.c

/*
 * above function but blocking
 */
extern struct tspckt *send_packet_to_socket_blocking(uint32_t port, void* buffer, size_t buffer_size); // socket.c

/*
 * validata a tspckt based on its identifier (checksum, hash, idk man)
 */
extern bool validate_tspckt(struct tspckt* packet); // tspctk.c

#endif //__LIGHTHOUSE_OS_CORE__
