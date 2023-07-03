#ifndef __ANIVA_IPC_TSPCKT__
#define __ANIVA_IPC_TSPCKT__
#include <libk/stddef.h>
#include "libk/error.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/socket.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"

struct thread;
struct threaded_socket;

typedef struct tspckt {
  struct thread* m_sender_thread;
  struct threaded_socket* m_reciever_thread;

  uint32_t m_identifier; // checksum? (like some form of security in this system lmao)
  uint32_t m_reserved0;

  packet_payload_t* m_payload;
  size_t m_packet_size;
} tspckt_t;

/* Initialize the allocator for tspackets */
void init_tspckt();

/*
 * allocate a tspckt on the (current) heap
 */
tspckt_t *create_tspckt(struct threaded_socket* destination, driver_control_code_t code, void* data, size_t data_size);

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
