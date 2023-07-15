#ifndef __ANIVA_PACKET_RESPONSE__
#define __ANIVA_PACKET_RESPONSE__
#include "libk/flow/error.h"
#include <libk/stddef.h>

/*
 * Layout of the packet response structure and management
 *
 * The packet response is a cache structure that will be stacked up per thread. Every thread is
 * capable of recieving responses, and thus when a normal thread sends a packet, it can poll itself for
 * a response, by simply checking its cache to see if something has arived that it is interested in
 */
typedef struct packet_response {
  void* m_response_buffer;
  size_t m_response_size;
} packet_response_t;

void init_packet_response();

packet_response_t* create_packet_response(void* data, size_t size);
void destroy_packet_response(packet_response_t* response);

#endif // !__ANIVA_PACKET_RESPONSE__
