#include "packet_payload.h"
#include "mem/heap.h"
#include <libk/string.h>

packet_payload_t* create_packet_payload(void* data, size_t size) {
  packet_payload_t* payload = kmalloc(sizeof(packet_payload_t));
  payload->m_data_size = size;
  payload->m_data = kmalloc(size);
  memcpy(payload->m_data, data, size);
  return payload;
}

void destroy_packet_payload(packet_payload_t* payload) {
  kfree(payload->m_data);
  kfree(payload);
}
