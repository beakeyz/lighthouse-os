#include "socket.h"
#include <mem/kmalloc.h>
#include <system/asm_specifics.h>
#include "core.h"
#include "thread.h"
#include "proc/ipc/tspckt.h"
#include "libk/string.h"

threaded_socket_t *create_threaded_socket(thread_t* parent, uint32_t port, size_t max_size_per_buffer) {

  if (max_size_per_buffer <= MIN_SOCKET_BUFFER_SIZE) {
    return nullptr;
  }

  threaded_socket_t *ret = kmalloc(sizeof(threaded_socket_t));
  ret->m_port = port;
  ret->m_state = THREADED_SOCKET_STATE_LISTENING;
  ret->m_max_size_per_buffer = max_size_per_buffer;
  ret->m_buffers = create_queue(SOCKET_DEFAULT_MAXIMUM_BUFFER_COUNT);

  // NOTE: parent should be nullable
  ret->m_parent = parent;

  // TODO: add to socket list
  if (vector_add(g_sockets, ret) == ANIVA_FAIL) {

    vector_ensure_capacity(g_sockets, g_sockets->m_max_capacity + 1);
    if (vector_add(g_sockets, ret) == ANIVA_FAIL) {
      kfree(ret->m_buffers);
      kfree(ret);
      return nullptr;
    }
  }

  return ret;
}

ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr) {

  // TODO: remove from socket list
  uintptr_t idx = Must(vector_indexof(g_sockets, ptr));
  if (vector_remove(g_sockets, idx) == ANIVA_FAIL) {
    return ANIVA_FAIL;
  }

  kfree(ptr->m_buffers);
  kfree(ptr);
  return ANIVA_SUCCESS;
}

threaded_socket_t *find_socket(uint32_t port) {
  FOREACH(i, &g_sockets->m_items) {
    threaded_socket_t *socket = i->data;
    if (socket && socket->m_port == port) {
      return socket;
    }
  }
  return nullptr;
}

ErrorOrPtr send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  // TODO: list lookup
  threaded_socket_t *socket = find_socket(port);

  if (socket->m_parent == nullptr) {
    return Error();
  }

  tspckt_t *packet = create_tspckt(buffer, buffer_size);

  // don't allow buffer restriction violations
  if (packet->m_packet_size > socket->m_max_size_per_buffer) {
    destroy_tspckt(packet);
    return Error();
  }

  queue_enqueue(socket->m_buffers, packet);

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return Success((uintptr_t)&packet->m_response);
}

tspckt_t *send_packet_to_socket_blocking(uint32_t port, void* buffer, size_t buffer_size) {

  ErrorOrPtr result = send_packet_to_socket(port, buffer, buffer_size);
  if (result.m_status == ANIVA_FAIL) {
    return nullptr;
  }

  tspckt_t **packet_dptr = (tspckt_t**) Release(result);

  for (;;) {
    tspckt_t *response_ptr = (tspckt_t*)*packet_dptr;
    if (response_ptr != nullptr) {
      return response_ptr;
    }
  }
}
