#include "socket.h"
#include <mem/heap.h>
#include <system/asm_specifics.h>
#include "core.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/data/linkedlist.h"
#include "libk/data/queue.h"
#include "libk/flow/reference.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "mem/pg.h"
#include "mem/zalloc.h"
#include "proc/context.h"
#include "proc/ipc/packet_response.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"
#include "thread.h"
#include "proc/ipc/tspckt.h"
#include "interrupts/interrupts.h"
#include <mem/heap.h>

static zone_allocator_t* __socket_allocator;

static ALWAYS_INLINE void reset_socket_flags(threaded_socket_t* ptr);

threaded_socket_t *create_threaded_socket(thread_t* parent, FuncPtr exit_fn, SocketOnPacket on_packet_fn, uint32_t port, size_t max_size_per_buffer) {

  threaded_socket_t *ret;

  if (max_size_per_buffer <= MIN_SOCKET_BUFFER_SIZE)
    return nullptr;

  ret = zalloc_fixed(__socket_allocator);

  if (!ret)
    return nullptr;

  ret->m_port = port;
  ret->m_socket_flags = NULL;
  ret->m_packet_queue.m_packets = create_limitless_queue();
  ret->m_packet_queue.m_lock = create_spinlock();
  ret->f_on_packet = on_packet_fn;
  ret->f_exit_fn = exit_fn;
  ret->m_state = THREADED_SOCKET_STATE_LISTENING;
  ret->m_max_size_per_buffer = max_size_per_buffer;

  ret->m_packet_mutex = create_mutex(0);

  // NOTE: parent should be nullable
  ret->m_parent = parent;

  reset_socket_flags(ret);

  if (on_packet_fn == nullptr) {
    socket_set_flag(ret, TS_IS_CLOSED, true);
  }

  socket_register(ret);

  return ret;
}

ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr) {

  if (socket_unregister(ptr).m_status == ANIVA_FAIL) {
    return ANIVA_FAIL;
  }

  destroy_mutex(ptr->m_packet_mutex);
  zfree_fixed(__socket_allocator, ptr);
  return ANIVA_SUCCESS;
}

/*
 * send a data-packet to a port
 * returns a pointer to the response ptr (thus a double pointer)
 * if all goes well, otherwise a nullptr
 * 
 * this mallocs the buffer for the caller, so caller is responsible for cleaning up 
 * their own mess
 */
ErrorOrPtr send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size) {
  return send_packet_to_socket_ex(port, DCC_DATA, buffer, buffer_size);
}

/*!
 * @brief Extended function that 'sends' packets to sockets
 *
 * Tries to lock the spinlock of the socket (NOTE: returns a warning if 
 * the mutex is already lock, as to support sending packets in irqs) 
 * and then adds a tspacket to the queue of the socket
 */
ErrorOrPtr send_packet_to_socket_ex(uint32_t port, driver_control_code_t code, void* buffer, size_t buffer_size) {

  threaded_socket_t *socket = find_registered_socket(port);

  if (socket == nullptr || socket->m_parent == nullptr) {
    return Error();
  }

  if (spinlock_is_locked(socket->m_packet_queue.m_lock))
    return Warning();

  spinlock_lock(socket->m_packet_queue.m_lock);

  tspckt_t *packet = create_tspckt(socket, code, buffer, buffer_size);

  if (!packet)
    return Error();

  // don't allow buffer restriction violations
  if (packet->m_packet_size > socket->m_max_size_per_buffer) {
    destroy_tspckt(packet);
    spinlock_unlock(socket->m_packet_queue.m_lock);
    return Error();
  }

  queue_enqueue(socket->m_packet_queue.m_packets, packet);

  spinlock_unlock(socket->m_packet_queue.m_lock);

  return Success(0);
}

ErrorOrPtr socket_enable(struct thread* socket) {

  if (!socket || !thread_is_socket(socket))
    return Error();

  socket_set_flag(socket->m_socket, TS_READY, true);

  return Success(0);
}

ErrorOrPtr socket_disable(struct thread* socket) {

  if (!socket || !thread_is_socket(socket))
    return Error();

  socket_set_flag(socket->m_socket, TS_READY, false);

  return Success(0);
}


void socket_set_flag(threaded_socket_t *ptr, THREADED_SOCKET_FLAGS_t flag, bool value) {
  if (value) {
    ptr->m_socket_flags |= flag;
  } else {
    ptr->m_socket_flags &= ~(flag);
  }
}

bool socket_is_flag_set(threaded_socket_t* ptr, THREADED_SOCKET_FLAGS_t flag) {
  if (ptr == nullptr) {
    return false;
  }
  return (ptr->m_socket_flags & flag) != 0;
}

ErrorOrPtr socket_register_pckt_func(SocketOnPacket fn)
{
  thread_t* current_thread;
  threaded_socket_t* socket;

  if (!fn)
    return Error();

  current_thread = get_current_scheduling_thread();

  if (!current_thread || !thread_is_socket(current_thread))
    return Error();

  socket = current_thread->m_socket;

  ASSERT_MSG(socket, "WHAT: thread marked as socket but didn't have one?");

  /* Lock the mutext to ensure we are allowed to modify the function */
  mutex_lock(socket->m_packet_mutex);

  socket->f_on_packet = fn;

  mutex_unlock(socket->m_packet_mutex);

  return Success(0);
}


static ALWAYS_INLINE void reset_socket_flags(threaded_socket_t* ptr) {
  ptr->m_socket_flags = 0;
}

ErrorOrPtr socket_try_handle_tspacket(threaded_socket_t* socket) {

  tspckt_t* packet;

  /* 0) Validate the socket */
  if (!socket || !socket->m_parent)
    return Error();

  if (!socket_is_flag_set(socket, TS_READY))
    return Error();

  /* 1) See if there is a packet for us (in the sockets queue) */
  packet = queue_peek(socket->m_packet_queue.m_packets);

  if (!validate_tspckt(packet) || packet->m_reciever_thread != socket)
    return Error();

  /* 2) Yoink it and handle it */
  ErrorOrPtr result = socket_handle_tspacket(packet);

  queue_dequeue(socket->m_packet_queue.m_packets);
  if (result.m_status == ANIVA_WARNING) {
    // packet was not ready, delay
    queue_enqueue(socket->m_packet_queue.m_packets, packet);
    return Error();
  }

  return Success(0);
}

static bool __is_socket_ready(threaded_socket_t* socket)
{
  return (
      socket &&
      socket->m_packet_mutex &&
      !socket_is_flag_set(socket, TS_SHOULD_EXIT) &&
      socket_is_flag_set(socket, TS_READY) &&
      !mutex_is_locked(socket->m_packet_mutex) &&
      socket->f_on_packet);
}

ErrorOrPtr socket_handle_tspacket(tspckt_t* packet) {

  // no valid tspacket from the queue,
  // so we bail
  if (!validate_tspckt(packet)) {
    return Error();
  }

  threaded_socket_t* socket = packet->m_reciever_thread;

  // don't handle any packets when this socket is not available
  if (!__is_socket_ready(socket)) {
    println("socket not ready: ");
    println(socket->m_parent->m_name);
    return Warning();
  }

  println(socket->m_parent->m_name);

  thread_t* thread = socket->m_parent;

  ASSERT_MSG(thread != nullptr, "Found a socket without a parent thread!");

  /* Copy the payload onto the stack */
  packet_payload_t payload = packet->m_payload;
  /* Create a placeholder pointer for the response */
  packet_response_t* response = nullptr;

  // FIXME: Lock the mutex so we can do whatever
  // mutex_lock(socket->m_packet_mutex);

  //thread_set_state(thread, RUNNING);

  switch (payload.m_code) {
    case DCC_EXIT:
      socket_set_flag(thread->m_socket, TS_SHOULD_EXIT, true);
      break;
    case DCC_RESPONSE:

      kernel_panic("TODO: responses");
      /* This packet is a response to an earlier sent packet */
      break;
    default:
      {
        // TODO: can we put this result in the response?
        SocketOnPacket sop = (SocketOnPacket)thread->m_socket->f_on_packet;
        sop(payload, &response);
        break;
      }
  }

  if (response != nullptr) {

    send_packet_to_socket_ex(T_SOCKET(packet->m_sender_thread)->m_port, DCC_RESPONSE, response->m_response_buffer, response->m_response_size);

    destroy_packet_response(response);
  }

  destroy_tspckt(packet);

  // we want to keep this packet alive for now, all the way untill the response has been handled
  // we jump here when we are done handeling a potential socketroutine

  // no need to unlock the mutex, because it just gets
  // deleted lmao

  // NOTE: this mutex MUST be unlocked after we are done with this packet, otherwise 
  // the socket will deadlock
  // FIXME: locking?
  // mutex_unlock(socket->m_packet_mutex);
  return Success(0);
}

void init_socket()
{
  __socket_allocator = create_zone_allocator(16 * Kib, sizeof(threaded_socket_t), NULL);

  ASSERT_MSG(__socket_allocator, "Failed to create socket allocator!");
}
