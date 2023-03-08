#include "packet_payload.h"
#include "mem/heap.h"

packet_payload_t* create_packet_payload(void* data, size_t size, uint32_t source_port) {
  packet_payload_t* payload = kmalloc(sizeof(packet_payload_t));
  payload->m_source_port = source_port;
  payload->m_data = data;
  payload->m_data_size = size;
  return payload;
}

void destroy_packet_payload(packet_payload_t* payload) {
  kfree(payload->m_data);
  kfree(payload);
}
