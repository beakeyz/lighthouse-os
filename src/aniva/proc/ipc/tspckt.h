#ifndef __ANIVA_IPC_TSPCKT__
#define __ANIVA_IPC_TSPCKT__
#include <libk/stddef.h>
#include "libk/error.h"

struct thread;

typedef struct tspckt {
  uint32_t m_identifier; // checksum? (like some form of security in this system lmao)
  struct thread* m_sender_thread;
  struct tspckt* m_response; // FIXME: atomic?
  size_t m_packet_size;
  void* m_data;
} tspckt_t;

/*
 * allocate a tspckt on the (current) heap
 */
tspckt_t *create_tspckt(void* data, size_t data_size);

/*
 * create invalid tspckt that would not pass
 * "validate_tspckt" (core.h)
 */
tspckt_t *create_invalid_tspckt();

/*
 * prepare a tspckt where the caller is responsible
 * for allocation
 */
ANIVA_STATUS prepare_tspckt(tspckt_t* packet, void* data, size_t data_size);

/*
 * destroy a tspckt instance and pointer
 */
ANIVA_STATUS destroy_tspckt(tspckt_t* packet);

/*
 * Wait for either a response or confirmation that the
 * packet is handled. Thread will technically be blocked,
 * but not marked as such
 */
ANIVA_STATUS tspckt_wait_for_response(tspckt_t* packet);

/*
 * same as above, but with a timeout value in milliseconds
 * FIXME: there is no sleep function in thread.c yet so yea =/
 */
ANIVA_STATUS tspckt_wait_for_response_with_timeout(tspckt_t* packet, size_t timeout);

/*
 * Check if the current packet has a response
 */
ANIVA_STATUS tspckt_check_for_response(tspckt_t* packet);

#endif //__ANIVA_IPC_TSPCKT__
