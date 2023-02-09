#include "socket.h"
#include <mem/kmalloc.h>
#include <system/asm_specifics.h>
#include "core.h"
#include "thread.h"
#include "proc/ipc/tspckt.h"
#include "interupts/interupts.h"

static ALWAYS_INLINE void reset_socket_flags(threaded_socket_t* ptr);

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

  reset_socket_flags(ret);

  socket_register(ret);

  return ret;
}

ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr) {

  if (socket_unregister(ptr).m_status == ANIVA_FAIL) {
    return ANIVA_FAIL;
  }

  kfree(ptr->m_buffers);
  kfree(ptr);
  return ANIVA_SUCCESS;
}

// FIXME: should this disable interrupts?
ErrorOrPtr send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  // TODO: list lookup
  threaded_socket_t *socket = find_registered_socket(port);

  if (socket == nullptr || socket->m_parent == nullptr) {
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

void socket_set_flag(threaded_socket_t *ptr, THREADED_SOCKET_FLAGS_t flag, bool value) {
  if (value) {
    ptr->m_socket_flags |= flag;
  } else {
    ptr->m_socket_flags &= ~(flag);
  }
}

bool socket_is_flag_set(threaded_socket_t* ptr, THREADED_SOCKET_FLAGS_t flag) {
  return (ptr->m_socket_flags & flag) == true;
}


static ALWAYS_INLINE void reset_socket_flags(threaded_socket_t* ptr) {
  ptr->m_socket_flags = 0;
}