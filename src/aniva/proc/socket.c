#include "socket.h"
#include <mem/kmalloc.h>
#include <system/asm_specifics.h>

threaded_socket_t *create_threaded_socket(uint32_t port, size_t buffer_size) {

  if (buffer_size <= MIN_SOCKET_BUFFER_SIZE) {
    return nullptr;
  }

  threaded_socket_t *ret = kmalloc(sizeof(threaded_socket_t));
  ret->m_port = port;
  ret->m_state = THREADED_SOCKET_STATE_LISTENING;
  ret->m_buffer_size = buffer_size;
  ret->m_buffer = kmalloc(buffer_size);

  // TODO: add to socket list

  return ret;
}

ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr) {

  // TODO: remove from socket list

  kfree(ptr->m_buffer);
  kfree(ptr);
  return ANIVA_SUCCESS;
}

ANIVA_STATUS send_packet_to_socket(uint32_t port, void* buffer) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  // TODO: list lookup

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
}