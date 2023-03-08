#ifndef __ANIVA_IPC_TSPCKT__
#define __ANIVA_IPC_TSPCKT__
#include <libk/stddef.h>
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "sync/mutex.h"

struct thread;
struct threaded_socket;

typedef struct tspckt {
  uint32_t m_identifier; // checksum? (like some form of security in this system lmao)
  struct thread* m_sender_thread;
  struct threaded_socket* m_reciever_thread;
  mutex_t* m_packet_mutex;

  async_ptr_t* m_response_ptr;
  volatile packet_response_t* m_response;
  packet_payload_t* m_payload;
  size_t m_packet_size;
} tspckt_t;

/*
 * allocate a tspckt on the (current) heap
 */
tspckt_t *create_tspckt(struct threaded_socket* destination, void* data, size_t data_size);

/*
 * create invalid tspckt that would not pass
 * "validate_tspckt" (core.h)
 */
tspckt_t *create_invalid_tspckt();

/*
 * destroy a tspckt instance and pointer
 */
ANIVA_STATUS destroy_tspckt(tspckt_t* packet);

#endif //__ANIVA_IPC_TSPCKT__
