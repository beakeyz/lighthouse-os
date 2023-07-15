#include "packet_payload.h"
#include "dev/debug/serial.h"
#include "mem/heap.h"
#include <libk/string.h>

void init_packet_payload(packet_payload_t* payload, struct tspckt* parent, void* data, size_t size, driver_control_code_t code) {

  if (!payload)
    return;

  payload->m_data_size = size;
  payload->m_code = code;
  payload->m_data = data;
  payload->m_parent = parent;

}

void destroy_packet_payload(packet_payload_t* payload) {

  //if (payload->m_data)
  //  kfree(payload->m_data);

  //kfree(payload);
  (void)payload;
}

void init_packet_payloads()
{

}
