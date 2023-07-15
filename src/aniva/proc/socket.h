#ifndef __ANIVA_SOCKET_THREAD__
#define __ANIVA_SOCKET_THREAD__
#include <libk/stddef.h>
#include <libk/flow/error.h>
#include "libk/data/queue.h"
#include "proc/context.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "sync/spinlock.h"
#include "system/processor/registers.h"

struct mutex;
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
  TS_SHOULD_EXIT = (1 << 4),// the socket has recieved the command to exit
  TS_READY = (1 << 5),      // the socket is ready to recieve packets
  TS_SHOULD_HANDLE_USERPACKET = (1 << 6),
  TS_HANDLING_USERPACKET = (1 << 7), // the socket is busy in userspace, trying to handle a packet
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
  size_t m_max_size_per_buffer;

  uint32_t m_port;
  uint32_t m_socket_flags;

  thread_context_t m_old_context;

  SocketOnPacket f_on_packet;
  FuncPtr f_exit_fn;

  socket_packet_queue_t m_packet_queue;

  THREADED_SOCKET_STATE_t m_state;

  struct mutex* m_packet_mutex;
  struct thread* m_parent;
} threaded_socket_t;

void init_socket();

/*
 * create a socket in a thread
 */
threaded_socket_t *create_threaded_socket(struct thread* parent, FuncPtr exit_fn, SocketOnPacket on_packet_fn, uint32_t port, size_t max_size_per_buffer);

/*
 * destroy a socket and its resources
 */
ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr);

ErrorOrPtr socket_enable(struct thread* socket);
ErrorOrPtr socket_disable(struct thread* socket);

/* Register a I/O function for the current socket */
ErrorOrPtr socket_register_pckt_func(SocketOnPacket fn);

/*
 * control the flags of a socket
 */
void socket_set_flag(threaded_socket_t *ptr, THREADED_SOCKET_FLAGS_t flag, bool value);

/*
 * get the value of a flag
 */
bool socket_is_flag_set(threaded_socket_t* ptr, THREADED_SOCKET_FLAGS_t flag);

bool socket_has_userpacket_handler(threaded_socket_t* socket);

/*
 * See if there is a packet lying around for us and if so, we 
 * handle it and yeet it out
 */
ErrorOrPtr socket_try_handle_tspacket(threaded_socket_t* socket);

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

