#ifndef __ANIVA_SOCKET_THREAD__
#define __ANIVA_SOCKET_THREAD__
#include <libk/stddef.h>
#include <libk/error.h>
#include "libk/queue.h"

struct thread;
struct tspckt;
struct threaded_socket;

typedef enum THREADED_SOCKET_STATE {
  THREADED_SOCKET_STATE_LISTENING = 0,
  THREADED_SOCKET_STATE_BUSY,
  THREADED_SOCKET_STATE_BLOCKED
} THREADED_SOCKET_STATE_t;

typedef struct threaded_socket {
  uint32_t m_port;
  size_t m_max_size_per_buffer;
  queue_t* m_buffers;
  THREADED_SOCKET_STATE_t m_state;
  struct thread* m_parent;
} threaded_socket_t;

/*
 * create a socket in a thread
 */
threaded_socket_t *create_threaded_socket(struct thread* parent, uint32_t port, size_t max_size_per_buffer);

/*
 * destroy a socket and its resources
 */
ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr);

/*
 * find a socket based on its port
 * TODO: validate port based on checksum?
 */
threaded_socket_t *find_socket(uint32_t port);

// TODO: with timeout?

#endif

