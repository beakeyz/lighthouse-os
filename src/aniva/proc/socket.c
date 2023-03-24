#include "socket.h"
#include <mem/heap.h>
#include <system/asm_specifics.h>
#include "core.h"
#include "dev/debug/serial.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/linkedlist.h"
#include "libk/queue.h"
#include "libk/reference.h"
#include "libk/string.h"
#include "proc/ipc/packet_response.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include "system/processor/processor.h"
#include "thread.h"
#include "proc/ipc/tspckt.h"
#include "interupts/interupts.h"
#include <mem/heap.h>

static ALWAYS_INLINE void reset_socket_flags(threaded_socket_t* ptr);

threaded_socket_t *create_threaded_socket(thread_t* parent, FuncPtr exit_fn, SocketOnPacket on_packet_fn, uint32_t port, size_t max_size_per_buffer) {

  if (max_size_per_buffer <= MIN_SOCKET_BUFFER_SIZE) {
    return nullptr;
  }

  threaded_socket_t *ret = kmalloc(sizeof(threaded_socket_t));
  ret->m_port = port;
  ret->m_exit_fn = exit_fn;
  ret->m_on_packet = on_packet_fn;
  ret->m_packet_mutex = create_mutex(0);
  ret->m_state = THREADED_SOCKET_STATE_LISTENING;
  ret->m_max_size_per_buffer = max_size_per_buffer;

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
  kfree(ptr);
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
async_ptr_t** send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size) {
  return send_packet_to_socket_with_code(port, 0, buffer, buffer_size);
}

void send_packet_to_socket_no_response(uint32_t port, driver_control_code_t code, void* buffer, size_t buffer_size) {

  disable_interrupts();

  threaded_socket_t *socket = find_registered_socket(port);

  if (socket == nullptr || socket->m_parent == nullptr) {
    return;
  }

  tspckt_t *packet = create_tspckt(socket, code, buffer, buffer_size, create_async_ptr(socket->m_port));

  // don't allow buffer restriction violations
  if (packet->m_packet_size > socket->m_max_size_per_buffer) {
    destroy_tspckt(packet);
    destroy_async_ptr(&packet->m_async_ptr_handle);
    return;
  }

  //queue_enqueue(socket->m_buffers, packet);
  queue_enqueue(get_current_processor()->m_packet_queue.m_packets, packet);

  destroy_async_ptr(&packet->m_async_ptr_handle);
  packet->m_response_buffer = 0;

  enable_interrupts();
}

async_ptr_t** send_packet_to_socket_with_code(uint32_t port, driver_control_code_t code, void* buffer, size_t buffer_size) {
  // FIXME: is it necceserry to enter a critical section here?
  threaded_socket_t *socket = find_registered_socket(port);

  if (socket == nullptr || socket->m_parent == nullptr) {
    return nullptr;
  }

  tspckt_t *packet = create_tspckt(socket, code, buffer, buffer_size, create_async_ptr(socket->m_port));

  // don't allow buffer restriction violations
  if (packet->m_packet_size > socket->m_max_size_per_buffer) {
    destroy_tspckt(packet);
    destroy_async_ptr(&packet->m_async_ptr_handle);
    return nullptr;
  }

  //queue_enqueue(socket->m_buffers, packet);
  queue_enqueue(get_current_processor()->m_packet_queue.m_packets, packet);

  return &packet->m_async_ptr_handle;
}

packet_response_t send_packet_to_socket_blocking(uint32_t port, void* buffer, size_t buffer_size) {

  packet_response_t response = {0};
  async_ptr_t* result = *send_packet_to_socket(port, buffer, buffer_size);

  if (!result) {
    return response;
  }

  // await already destroys the async_ptr_t
  packet_response_t* response_result = await(result);

  response = *response_result;

  destroy_packet_response(response_result);

  return response;
}

void default_socket_entry_wrapper(uintptr_t args, thread_t* thread) {

  // pre-entry

  ASSERT_MSG(thread->m_socket != nullptr, "Started a thread as a socket, whilst it does not act like one (thread->m_socket == nullptr)");
  //ASSERT_MSG(buffer != nullptr, "Could not find a buffer while starting a socket");

  // call the actual entrypoint of the thread
  // NOTE: they way this works now is that we only
  // dequeue packets after the entry function has ended.
  // this is bad, so we need to handle dequeueing on a different thread...
  socket_set_flag(thread->m_socket, TS_ACTIVE, true);
  thread->m_real_entry(args);
  socket_set_flag(thread->m_socket, TS_READY, true);

  // when the entry of a socket exits, we 
  // idle untill we recieve signals
  //thread_set_state(thread, SLEEPING);

  for (;;) {
    if (socket_is_flag_set(thread->m_socket, TS_SHOULD_EXIT)) {
      break;
    }
    // TODO: implement acurate sleeping
    // NOTE: we need to sleep here, in such a way that we do allow context switches WHILE keeping interrupts enabled...
    delay(1000);
  }

  // if we break this loop, we finally exit the thread
  thread->m_socket->m_exit_fn();

  // signal the scheduler we want to get cleaned up
  thread_set_state(thread, DYING);

  // yield will enable interrupts again
  scheduler_yield();
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


static ALWAYS_INLINE void reset_socket_flags(threaded_socket_t* ptr) {
  ptr->m_socket_flags = 0;
}

ErrorOrPtr socket_handle_tspacket(tspckt_t* packet) {

  // no valid tspacket from the queue,
  // so we bail
  if (!validate_tspckt(packet)) {
    return Error();
  }

  threaded_socket_t* socket = packet->m_reciever_thread;

  // don't handle any packets when this socket is not available
  if (socket_is_flag_set(socket, TS_SHOULD_EXIT) || !socket_is_flag_set(socket, TS_READY)) {
    println("socket not ready: ");
    println(socket->m_parent->m_name);
    return Warning();
  }

  println(socket->m_parent->m_name);

  thread_t* thread = socket->m_parent;

  ASSERT_MSG(thread != nullptr, "Found a socket without a parent thread!");

  packet_payload_t payload = *packet->m_payload;
  packet_response_t* response = nullptr;
  uintptr_t on_packet_result;

  // Lock the mutex so we can do whatever
  mutex_lock(socket->m_packet_mutex);

  //thread_set_state(thread, RUNNING);
  const size_t data_size = payload.m_data_size;

  switch (payload.m_code) {
    case DCC_EXIT:
      socket_set_flag(thread->m_socket, TS_SHOULD_EXIT, true);
      break;
    default:
      // TODO: can we put this result in the response?
      on_packet_result = thread->m_socket->m_on_packet(payload, &response);
      break;
  }

  if (response != nullptr) {

    if (!packet->m_response_buffer || !packet->m_async_ptr_handle || !packet->m_async_ptr_handle->m_mutex) {
      destroy_packet_response(response);
    } else {
      *packet->m_response_buffer = response;
    }
  } else {
    if (packet->m_async_ptr_handle) {
      if (mutex_is_locked(packet->m_async_ptr_handle->m_mutex)) {
        atomic_ptr_write(packet->m_async_ptr_handle->m_is_buffer_killed, true);
      } else {
        destroy_async_ptr(&packet->m_async_ptr_handle);
      }
    }
  }

  // we want to keep this packet alive for now, all the way untill the response has been handled
  // we jump here when we are done handeling a potential socketroutine

  // no need to unlock the mutex, because it just gets
  // deleted lmao
  destroy_tspckt(packet);

  // NOTE: this mutex MUST be unlocked after we are done with this packet, otherwise 
  // the socket will deadlock
  mutex_unlock(socket->m_packet_mutex);
  return Success(0);
}

