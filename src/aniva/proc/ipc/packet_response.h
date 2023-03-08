#ifndef __ANIVA_PACKET_RESPONSE__
#define __ANIVA_PACKET_RESPONSE__
#include <libk/stddef.h>

typedef struct packet_response {
  void* m_response_buffer;
  size_t m_response_size;
  uint32_t m_source_port;
} packet_response_t;

packet_response_t* create_packet_response(void* data, size_t size, uint32_t source_port);
void destroy_packet_response(packet_response_t* request);


#endif // !__ANIVA_PACKET_RESPONSE__
