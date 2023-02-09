#include "core.h"
#include "socket.h"

static list_t *s_sockets = nullptr;

ANIVA_STATUS initialize_proc_core() {
  s_sockets = init_list();

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