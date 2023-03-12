#ifndef __ANIVA_SOCKET_THREAD__
#define __ANIVA_SOCKET_THREAD__
#include <libk/stddef.h>
#include <libk/error.h>
#include "libk/queue.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "sync/spinlock.h"

struct thread;
struct tspckt;
struct threaded_socket;

typedef enum THREADED_SOCKET_STATE {
  THREADED_SOCKET_STATE_LISTENING = 0,
  THREADED_SOCKET_STATE_BUSY,
  THREADED_SOCKET_STATE_BLOCKED,
} THREADED_SOCKET_STATE_t;

typedef enum THREADED_SOCKET_FLAGS {
  TS_REGISTERED = (1 << 0), // the socket has been registered
  TS_ACTIVE = (1 << 1),     // the socket is actively listening
  TS_BUSY = (1 << 2),       // the socket is busy and can't respond to requests (they might fail)
  TS_IS_CLOSED = (1 << 3),  // the socket has no callback function where packets can be passed to
  TS_SHOULD_EXIT = (1 << 4) // the socket has recieved the command to exit
} THREADED_SOCKET_FLAGS_t;

typedef uintptr_t (*SocketOnPacket) (
  packet_payload_t payload,
  packet_response_t** response
);

typedef struct socket_packet_queue {
  spinlock_t* m_lock;
  queue_t* m_packets;
} socket_packet_queue_t;

typedef struct threaded_socket {
  uint32_t m_port;
  size_t m_max_size_per_buffer;

  uintptr_t m_socket_flags;

  FuncPtr m_exit_fn;
  SocketOnPacket m_on_packet;

  THREADED_SOCKET_STATE_t m_state;

  struct thread* m_parent;
} threaded_socket_t;

/*
 * create a socket in a thread
 */
threaded_socket_t *create_threaded_socket(struct thread* parent, FuncPtr exit_fn, SocketOnPacket on_packet_fn, uint32_t port, size_t max_size_per_buffer);

/*
 * destroy a socket and its resources
 */
ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr);

/*
 * control the flags of a socket
 */
void socket_set_flag(threaded_socket_t *ptr, THREADED_SOCKET_FLAGS_t flag, bool value);

/*
 * get the value of a flag
 */
bool socket_is_flag_set(threaded_socket_t* ptr, THREADED_SOCKET_FLAGS_t flag);

/*
 * The default entry wrapper when creating sockets
 */ 
void default_socket_entry_wrapper(uintptr_t args, struct thread* thread);

/*
 * handle a packet
 */
ErrorOrPtr socket_handle_tspacket(struct tspckt* packet);

/*
 * Check if a port is valid (i.e. unused and within the bounds)
 * if it's not, the socket gets appointed a new port
 *
 * Returns: the port this socket lives on after the verification,
 * if it succeeded. otherwise an error
 */
extern ErrorOrPtr socket_try_verifiy_port(threaded_socket_t* socket);

/*
 * verifies the port of this socket and changes it if the 
 * verification fails. calls socket_try_verifiy_port under the hood
 */
extern uint32_t socket_verify_port(threaded_socket_t* socket);

/*
ErrorOrPtr socket_handle_packet(threaded_socket_t* socket);
void socket_handle_packets(threaded_socket_t* socket);
*/
// TODO: with timeout?

#endif

