#ifndef __LIGHTENV_LIBSYS_IPC_SOCKET_DEF__
#define __LIGHTENV_LIBSYS_IPC_SOCKET_DEF__

#include <sys/types.h>
#include <lightos/handle_def.h>

struct ipc_packet;

/*
 * Socket definition
 *
 * It needs to know about:
 *  - The processes/threads it's passing between
 *  - The maximum size of it's packets
 *
 * NOTE: if the frequency of packet transfers gets too high, there may occur a loss of data.
 *       for high frequency transfers rates, one may look into shared memory regions
 */
typedef struct ipc_socket {
  HANDLE partner_proc;
  uint32_t packet_size;
  uint32_t packet_capacity;
  /* We make use of a simple ringbuffer */
  uint32_t packet_tx_idx;
  uint32_t packet_rx_idx;
  struct ipc_packet* packet_buffer;
} ipc_socket_t;

/*
 * Packet size is defined by the socket it's used by
 *
 * NOTE: packets NEVER own the memory of their data
 */
typedef struct ipc_packet {
  uint32_t timestamp;
  uint8_t data[];
} ipc_packet_t;

#endif // !__LIGHTENV_LIBSYS_IPC_SOCKET_DEF__
