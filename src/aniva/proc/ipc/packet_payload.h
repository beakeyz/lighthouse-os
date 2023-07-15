#ifndef __ANIVA_PACKET_PAYLOAD__
#define __ANIVA_PACKET_PAYLOAD__
#include "dev/core.h"
#include <libk/stddef.h>

/*
 * TODO: this may probably also keep track of the response buffers and size, 
 * since that might be handy for doing quick response copying
 */

struct tspckt;

/*
 * Layout of the packet payload structure and management
 *
 * A payload is a structure that gets embedded into every tspacket that gets sent. Therefor
 * we really never want to allocate a standalone packet_payload anywhere, hence why we don't have
 * a allocator for it. It simply gets created and destroyed together with the packet.
 * 
 * 
 */
typedef struct packet_payload {
  void* m_data;
  size_t m_data_size;
  driver_control_code_t m_code;
  struct tspckt* m_parent;
} packet_payload_t;

void init_packet_payloads();

void init_packet_payload(packet_payload_t* payload, struct tspckt* parent, void* data, size_t size, driver_control_code_t code);
void destroy_packet_payload(packet_payload_t* payload);

#endif // !__ANIVA_PACKET_PAYLOAD__
