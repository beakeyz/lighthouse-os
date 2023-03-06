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

typedef enum THREADED_SOCKET_FLAGS {
  TS_REGISTERED = (1 << 0),
  TS_ACTIVE = (1 << 1),
  TS_BUSY = (1 << 2)
} THREADED_SOCKET_FLAGS_t;

typedef struct threaded_socket {
  uint32_t m_port;
  size_t m_max_size_per_buffer;

  uintptr_t m_socket_flags;

  // FIXME: have a socket-specific buffer_queue struct that we can pass to threads
  queue_t* m_buffers;
  FuncPtr m_exit_fn;

  THREADED_SOCKET_STATE_t m_state;

  struct thread* m_parent;
} threaded_socket_t;

/*
 * create a socket in a thread
 */
threaded_socket_t *create_threaded_socket(struct thread* parent, FuncPtr exit_fn, uint32_t port, size_t max_size_per_buffer);

/*
 * destroy a socket and its resources
 */
ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr);

/*
 * control the flags of a socket
 */
void socket_set_flag(threaded_socket_t *ptr, THREADED_SOCKET_FLAGS_t flag, bool value);

/*
 * get the value of a flag
 */
bool socket_is_flag_set(threaded_socket_t* ptr, THREADED_SOCKET_FLAGS_t flag);

/*
 * The default entry wrapper when creating sockets
 */ 
void default_socket_entry_wrapper(uintptr_t args, struct thread* thread);

// TODO: with timeout?

#endif

