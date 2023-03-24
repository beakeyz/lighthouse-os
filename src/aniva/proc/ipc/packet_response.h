#ifndef __ANIVA_PACKET_RESPONSE__
#define __ANIVA_PACKET_RESPONSE__
#include "libk/error.h"
#include <libk/stddef.h>

struct tspckt;

typedef struct packet_response {
  void* m_response_buffer;
  size_t m_response_size;
} packet_response_t;

packet_response_t* create_packet_response(void* data, size_t size);
void destroy_packet_response(packet_response_t* response);

#endif // !__ANIVA_PACKET_RESPONSE__
