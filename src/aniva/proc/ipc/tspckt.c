#include "tspckt.h"
#include "mem/kmalloc.h"
#include "libk/string.h"
#include <sched/scheduler.h>
#include <system/asm_specifics.h>
#include "interupts/interupts.h"

static uint32_t generate_tspckt_identifier(tspckt_t* tspckt) USED;

tspckt_t *create_tspckt(void* data, size_t data_size) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  const size_t packet_size = sizeof(tspckt_t) + data_size;
  tspckt_t *packet = kmalloc(sizeof(tspckt_t));

  packet->m_response = create_invalid_tspckt();
  packet->m_identifier = NULL; // TODO
  packet->m_packet_size = packet_size;
  packet->m_data = kmalloc(data_size);
  // copy the data into our own buffer
  memcpy(packet->m_data, data, data_size);

  packet->m_sender_thread = get_current_scheduling_thread();

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return packet;
}

// TODO: checks
ANIVA_STATUS prepare_tspckt(tspckt_t* packet, void* data, size_t data_size) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();

  const size_t packet_size = sizeof(tspckt_t) + data_size;
  packet->m_response = create_invalid_tspckt();
  packet->m_identifier = NULL; // TODO
  packet->m_packet_size = packet_size;

  // copy the data into our own buffer
  memcpy(packet->m_data, data, data_size);

  packet->m_sender_thread = get_current_scheduling_thread();

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return ANIVA_SUCCESS;
}

// TODO: work out what invalid exactly means
tspckt_t *create_invalid_tspckt() {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  tspckt_t *ret = kmalloc(sizeof(tspckt_t));
  ret->m_response = nullptr;
  ret->m_data = nullptr;
  ret->m_packet_size = sizeof(tspckt_t);
  ret->m_sender_thread = nullptr;
  ret->m_identifier = 0;
  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return ret;
}

ANIVA_STATUS destroy_tspckt(tspckt_t* packet) {
  // for now our only heap allocated stuff is our own instance,
  // so lets just get rid of that
  kfree(packet->m_response);
  kfree(packet->m_data);
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
  if (packet->m_sender_thread == nullptr) {
    return false;
  }
  return true;
}

uint32_t generate_tspckt_identifier(tspckt_t* tspckt) {
  uint32_t ident = 0;
  //
}