#include "packet_payload.h"
#include "mem/heap.h"
#include <libk/string.h>

packet_payload_t* create_packet_payload(void* data, size_t size, driver_control_code_t code) {
  packet_payload_t* payload = kmalloc(sizeof(packet_payload_t));
  payload->m_data_size = size;

  if (data == nullptr || size == 0) {
    return payload;
  }

  payload->m_data = kmalloc(size);
  payload->m_code = code;

  memcpy(payload->m_data, data, size);
  return payload;
}

void destroy_packet_payload(packet_payload_t* payload) {

  if (payload->m_data)
    kfree(payload->m_data);

  kfree(payload);
}
