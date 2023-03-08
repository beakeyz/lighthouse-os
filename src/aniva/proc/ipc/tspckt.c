#include "tspckt.h"
#include <mem/heap.h>
#include "dev/debug/serial.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "libk/string.h"
#include <sched/scheduler.h>
#include <system/asm_specifics.h>
#include "interupts/interupts.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include <mem/heap.h>

static uint32_t generate_tspckt_identifier(tspckt_t* tspckt) USED;

tspckt_t *create_tspckt(threaded_socket_t* reciever, void* data, size_t data_size, packet_response_t** response_buffer) {
  const size_t packet_size = sizeof(tspckt_t) + data_size;
  tspckt_t *packet = kmalloc(sizeof(tspckt_t));

  ASSERT_MSG(thread_is_socket(reciever->m_parent), "reciever thread is not a socket!");

  packet->m_sender_thread = get_current_scheduling_thread();
  packet->m_reciever_thread = reciever;
  //packet->m_response_ptr = create_async_ptr((void**)&packet->m_response, reciever->m_port);
  packet->m_identifier = NULL; // TODO
  packet->m_packet_size = packet_size;
  packet->m_response_buffer = response_buffer;
  packet->m_payload = create_packet_payload(data, data_size);

  return packet;
}

// TODO: work out what invalid exactly means
tspckt_t *create_invalid_tspckt() {
  tspckt_t *ret = kmalloc(sizeof(tspckt_t));
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
  
  //destroy_async_ptr(packet->m_response_ptr);
  destroy_packet_payload(packet->m_payload);
  kfree(packet);
  return ANIVA_SUCCESS;
}

bool validate_tspckt(struct tspckt* packet) {
  // FIXME: this sucks balls, replace this with an actual algorithm lol
  if (packet == nullptr || packet->m_sender_thread == nullptr) {
    return false;
  }
  return true;
}

uint32_t generate_tspckt_identifier(tspckt_t* tspckt) {
  uint32_t ident = 0;
  //
  return ident;
}
