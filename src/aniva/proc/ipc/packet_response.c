#include "packet_response.h"
#include "mem/heap.h"

packet_response_t* create_packet_response(void* data, size_t size, uint32_t source_port) {
  packet_response_t* response = kmalloc(sizeof(packet_response_t));
  response->m_source_port = source_port;
  response->m_response_size = size;
  response->m_response_buffer = data;
  return response;
}

void destroy_packet_response(packet_response_t* request) {
  kfree(request->m_response_buffer);
  kfree(request);
}
