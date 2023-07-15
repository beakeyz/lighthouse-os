#include "tspckt.h"
#include <mem/heap.h>
#include "crypto/k_crc32.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include <sched/scheduler.h>
#include <system/asm_specifics.h>
#include "interrupts/interrupts.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/socket.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include <mem/heap.h>

static zone_allocator_t* __tspckt_allocator;

tspckt_t *create_tspckt(threaded_socket_t* reciever, driver_control_code_t code, void* data, size_t data_size) {
  const size_t packet_size = sizeof(tspckt_t) + data_size;
  tspckt_t *packet = zalloc_fixed(__tspckt_allocator);

  ASSERT_MSG(thread_is_socket(reciever->m_parent), "reciever thread is not a socket!");

  packet->m_sender_thread = get_current_scheduling_thread();
  packet->m_reciever_thread = reciever;
  packet->m_packet_size = packet_size;
  // this handle should not be destroyed when we are destroying this tspacket
  
  init_packet_payload(&packet->m_payload, packet, data, data_size, code);

  // NOTE: the identifier should be initialized last, since it relies on 
  // payload and packet being non-null
  packet->m_identifier = 0;
  packet->m_identifier = Must(generate_tspckt_identifier(packet)); 

  return packet;
}

// TODO: work out what invalid exactly means
tspckt_t *create_invalid_tspckt() {
  tspckt_t *ret = zalloc_fixed(__tspckt_allocator);
  memset(ret, 0, sizeof(*ret));
  ret->m_packet_size = sizeof(tspckt_t);
  return ret;
}

ANIVA_STATUS destroy_tspckt(tspckt_t* packet) {
  // for now our only heap allocated stuff is our own instance,
  // so lets just get rid of that

  // NOTE: when a packet gets a response, it stays alive to serve as a midleman for that response.
  // this zombified version of our packet then gets cleaned up when the response is handled...
  // TODO: we could just copy all the nesecary stuff over to the response structure and then still clean up the packet...
  
  destroy_packet_payload(&packet->m_payload);

  zfree_fixed(__tspckt_allocator, packet);
  return ANIVA_SUCCESS;
}

bool validate_tspckt(struct tspckt* packet) {
  // FIXME: this sucks balls, replace this with an actual algorithm lol
  if (packet == nullptr || packet->m_sender_thread == nullptr) {
    return false;
  }

  ErrorOrPtr result = generate_tspckt_identifier(packet);

  if (IsError(result))
    return false;

  if (result.m_ptr != packet->m_identifier)
    return false;

  return true;
}

ErrorOrPtr generate_tspckt_identifier(tspckt_t* packet) {
  if (!packet || !packet->m_payload.m_data)
    return Error();

  tspckt_t copy = *packet;
  copy.m_identifier = 0;

  uint32_t packet_crc = kcrc32(&copy, sizeof(tspckt_t));
  uint32_t payload_crc = kcrc32(copy.m_payload.m_data, packet->m_payload.m_data_size);
  
  uint64_t total_crc = ((uint64_t)packet_crc << 32) | (payload_crc & 0xFFFFFFFFUL);

  return Success(kcrc32(&total_crc, sizeof(uint64_t)));
}


void init_tspckt()
{
  __tspckt_allocator = create_zone_allocator_ex(nullptr, NULL, 25 * SMALL_PAGE_SIZE, sizeof(tspckt_t), NULL);

  ASSERT_MSG(__tspckt_allocator, "Failed to create a tspckt allocator!");
}
