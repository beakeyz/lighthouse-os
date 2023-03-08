#include "packet_response.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "mem/heap.h"
#include "proc/ipc/tspckt.h"
#include <libk/string.h>

packet_response_t* create_packet_response(void* data, size_t size) {
  packet_response_t* response = kmalloc(sizeof(packet_response_t));
  response->m_response_size = size;
  response->m_packet_handle = nullptr;
  response->m_response_buffer = kmalloc(size);
  memcpy(response->m_response_buffer, data, size);
  return response;
}

void destroy_packet_response(packet_response_t* response) {
  ASSERT_MSG(response->m_packet_handle != nullptr, "Tried to destroy a packet response that does not have a packet handle!");

  // FIXME: remove this garbage
  response->m_packet_handle->m_response = nullptr;
  destroy_tspckt(response->m_packet_handle);

  kfree(response->m_response_buffer);
  kfree(response);
}
