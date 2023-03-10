#include "packet_response.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "mem/heap.h"
#include "proc/ipc/tspckt.h"
#include <libk/string.h>

packet_response_t* create_packet_response(void* data, size_t size) {
  packet_response_t* response = kmalloc(sizeof(packet_response_t));
  response->m_response_size = size;
  response->m_response_buffer = kmalloc(size);
  memcpy(response->m_response_buffer, data, size);
  return response;
}

ErrorOrPtr destroy_packet_response(packet_response_t* response) {
  if (!response) {
    return Error();
  }

  kfree(response->m_response_buffer);
  kfree(response);
  return Success(0);
}
