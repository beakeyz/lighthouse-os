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
#include <mem/heap.h>

static uint32_t generate_tspckt_identifier(tspckt_t* tspckt) USED;

tspckt_t *create_tspckt(threaded_socket_t* reciever, void* data, size_t data_size) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  const size_t packet_size = sizeof(tspckt_t) + data_size;
  tspckt_t *packet = kmalloc(sizeof(tspckt_t));

  ASSERT_MSG(thread_is_socket(reciever->m_parent), "reciever thread is not a socket!");

  packet->m_packet_mutex = create_mutex(0);
  packet->m_sender_thread = get_current_scheduling_thread();
  packet->m_reciever_thread = reciever;
  packet->m_response_ptr = create_async_ptr((void**)&packet->m_response, reciever->m_port);
  packet->m_identifier = NULL; // TODO
  packet->m_packet_size = packet_size;
  packet->m_response = nullptr;
  packet->m_payload = create_packet_payload(data, data_size);

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return packet;
}

// TODO: checks
ANIVA_STATUS prepare_tspckt(tspckt_t* packet, void* data, size_t data_size) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();

  const size_t packet_size = sizeof(tspckt_t) + data_size;
  packet->m_packet_mutex = create_mutex(0);
  packet->m_response = nullptr;
  packet->m_payload = create_packet_payload(data, data_size);
  packet->m_identifier = NULL; // TODO
  packet->m_packet_size = packet_size;

  packet->m_sender_thread = get_current_scheduling_thread();

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return ANIVA_SUCCESS;
}

// TODO: work out what invalid exactly means
tspckt_t *create_invalid_tspckt() {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  tspckt_t *ret = kmalloc(sizeof(tspckt_t));
  ret->m_response = nullptr;
  ret->m_payload = nullptr;
  ret->m_response = nullptr;
  ret->m_response_ptr = nullptr;
  ret->m_packet_size = sizeof(tspckt_t);
  ret->m_sender_thread = nullptr;
  ret->m_identifier = 0;
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return ret;
}

ANIVA_STATUS destroy_tspckt(tspckt_t* packet) {
  // for now our only heap allocated stuff is our own instance,
  // so lets just get rid of that
  destroy_async_ptr(packet->m_response_ptr);
  destroy_packet_payload(packet->m_payload);
  destroy_mutex(packet->m_packet_mutex);
  if (packet->m_response) {
    destroy_packet_response((packet_response_t*)packet->m_response);
  }
  kfree(packet);
  return ANIVA_SUCCESS;
}

ANIVA_STATUS tspckt_wait_for_response(tspckt_t* packet) {
  while (true) {
    if (packet->m_response != nullptr) {
      // check identifier
      return ANIVA_SUCCESS;
    }
  }
  return ANIVA_FAIL;
}

ANIVA_STATUS tspckt_wait_for_response_with_timeout(tspckt_t* packet, size_t timeout) {

  while (timeout) {
    if (packet->m_response) {
      // check identifier
      return ANIVA_SUCCESS;
    }

    // sleep 1ms
    timeout--;
  }
  return ANIVA_FAIL;
}

ANIVA_STATUS tspckt_check_for_response(tspckt_t* packet) {
  if (packet != nullptr && packet->m_response != nullptr) {
    return ANIVA_SUCCESS;
  }
  return ANIVA_FAIL;
}

bool validate_tspckt(struct tspckt* packet) {
  // FIXME: this sucks balls, get rid of it
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
