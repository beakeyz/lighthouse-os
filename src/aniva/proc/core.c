#include "core.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/queue.h"
#include "libk/vector.h"
#include "socket.h"
#include "sync/atomic_ptr.h"
#include "sync/spinlock.h"

static list_t *s_sockets;

static spinlock_t* s_core_socket_lock;

// TODO: fix this mechanism, it sucks
static atomic_ptr_t* next_proc_id;

/*
 * Initialize:
 *  - socket registry
 *  - proc_id generation
 */
ANIVA_STATUS initialize_proc_core() {

  s_sockets = init_list();
  s_core_socket_lock = create_spinlock();
  next_proc_id = create_atomic_ptr_with_value(1);

  return ANIVA_SUCCESS;
}

// FIXME: EWWWWWWW
ErrorOrPtr generate_new_proc_id() {
  uintptr_t next = atomic_ptr_load(next_proc_id);
  atomic_ptr_write(next_proc_id, next + 1);
  return Success(next);
}

list_t get_registered_sockets() {
  if (!s_sockets) {
    list_t ret = {0};
    return ret;
  }
  return *s_sockets;
}

ErrorOrPtr socket_register(threaded_socket_t* socket) {
  if (socket == nullptr) {
    return Error();
  }

  if (socket_is_flag_set(socket, TS_REGISTERED)) {
    return Error();
  }

  socket_set_flag(socket, TS_REGISTERED, true);
  list_append(s_sockets, socket);
  return Success(0);
}

ErrorOrPtr socket_unregister(threaded_socket_t* socket) {
  if (socket == nullptr) {
    return Error();
  }

  if (!socket_is_flag_set(socket, TS_REGISTERED)) {
    return Error();
  }

  socket_set_flag(socket, TS_REGISTERED, false);

  ErrorOrPtr result = list_indexof(s_sockets, socket);

  if (result.m_status == ANIVA_FAIL) {
    return Error();
  }

  list_remove(s_sockets, Release(result));
  return Success(0);
}

threaded_socket_t *find_registered_socket(uint32_t port) {
  FOREACH(i, s_sockets) {
    threaded_socket_t *socket = i->data;
    if (socket && socket->m_port == port) {
      return socket;
    }
  }
  return nullptr;
}
