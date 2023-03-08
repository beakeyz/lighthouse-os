#ifndef __ANIVA_PACKET_PAYLOAD__
#define __ANIVA_PACKET_PAYLOAD__
#include <libk/stddef.h>

typedef struct packet_payload {
  void* m_data;
  size_t m_data_size;
} packet_payload_t;

packet_payload_t* create_packet_payload(void* data, size_t size);
void destroy_packet_payload(packet_payload_t* payload);

#endif // !__ANIVA_PACKET_PAYLOAD__
