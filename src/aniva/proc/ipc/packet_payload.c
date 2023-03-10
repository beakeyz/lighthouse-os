#include "packet_payload.h"
#include "dev/debug/serial.h"
#include "mem/heap.h"
#include <libk/string.h>

packet_payload_t* create_packet_payload(void* data, size_t size, driver_control_code_t code) {
  packet_payload_t* payload = kmalloc(sizeof(packet_payload_t));
  payload->m_data_size = size;
  payload->m_code = code;
  payload->m_data = data;

  return payload;
}

void destroy_packet_payload(packet_payload_t* payload) {

  //if (payload->m_data)
  //  kfree(payload->m_data);

  kfree(payload);
}
