#include "socket.h"
#include <mem/heap.h>
#include <system/asm_specifics.h>
#include "core.h"
#include "dev/debug/serial.h"
#include "kmain.h"
#include "libk/async_ptr.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/queue.h"
#include "libk/string.h"
#include "proc/ipc/packet_response.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
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
  ret->m_state = THREADED_SOCKET_STATE_LISTENING;
  ret->m_max_size_per_buffer = max_size_per_buffer;
  ret->m_buffers = create_queue(SOCKET_DEFAULT_MAXIMUM_BUFFER_COUNT);

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

  tspckt_t* packet;
  while ((packet = queue_dequeue(ptr->m_buffers)) != nullptr) {
    destroy_tspckt(packet);
  }

  kfree(ptr->m_buffers);
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
async_ptr_t* send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size) {
  return send_packet_to_socket_with_code(port, 0, buffer, buffer_size);
}

async_ptr_t* send_packet_to_socket_with_code(uint32_t port, driver_control_code_t code, void* buffer, size_t buffer_size) {
  // TODO: list lookup
  threaded_socket_t *socket = find_registered_socket(port);

  if (socket == nullptr || socket->m_parent == nullptr) {
    return nullptr;
  }

  async_ptr_t* response_ptr = create_async_ptr(socket->m_port);
  tspckt_t *packet = create_tspckt(socket, code, buffer, buffer_size, (packet_response_t**)response_ptr->m_response_buffer);

  // don't allow buffer restriction violations
  if (packet->m_packet_size > socket->m_max_size_per_buffer) {
    destroy_tspckt(packet);
    destroy_async_ptr(response_ptr);
    return nullptr;
  }

  queue_enqueue(socket->m_buffers, packet);

  return response_ptr;
}

packet_response_t send_packet_to_socket_blocking(uint32_t port, void* buffer, size_t buffer_size) {

  packet_response_t response = {0};
  async_ptr_t* result = send_packet_to_socket(port, buffer, buffer_size);

  if (!result) {
    return response;
  }

  packet_response_t* response_result = await(result);

  response = *response_result;

  destroy_packet_response(response_result);

  return response;
}

void default_socket_entry_wrapper(uintptr_t args, thread_t* thread) {

  // pre-entry
  queue_t* buffer = (queue_t*)thread->m_socket->m_buffers;

  ASSERT_MSG(thread->m_socket != nullptr, "Started a thread as a socket, whilst it does not act like one (thread->m_socket == nullptr)");
  ASSERT_MSG(buffer != nullptr, "Could not find a buffer while starting a socket");

  // call the actual entrypoint of the thread
  // NOTE: they way this works now is that we only
  // dequeue packets after the entry function has ended.
  // this is bad, so we need to handle dequeueing on a different thread...
  thread->m_real_entry(args);

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

void socket_handle_packets(threaded_socket_t* socket) {

  if (socket_is_flag_set(socket, TS_SHOULD_EXIT)) {
    return;
  }

  thread_t* thread = socket->m_parent;

  ASSERT_MSG(thread != nullptr, "Found a socket without a parent thread!");

  queue_t* buffer = socket->m_buffers;

  while (true) {

    tspckt_t* packet = queue_dequeue(buffer);

    // no valid tspacket from the queue,
    // so we bail
    if (!validate_tspckt(packet)) {
      break;
    }

    packet_payload_t payload = *packet->m_payload;
    packet_response_t* response = nullptr;
    uintptr_t on_packet_result;

    //thread_set_state(thread, RUNNING);
    const size_t data_size = payload.m_data_size;

    switch (payload.m_code) {
      case DCC_EXIT:
        socket_set_flag(thread->m_socket, TS_SHOULD_EXIT, true);
        break;
      case 0:
        // TODO: can we put this result in the response?
        on_packet_result = thread->m_socket->m_on_packet(payload, &response);
        break;
    }

    if (response != nullptr) {
      *packet->m_response_buffer = response;
    }

    // we want to keep this packet alive for now, all the way untill the response has been handled
    // we jump here when we are done handeling a potential socketroutine

    // no need to unlock the mutex, because it just gets
    // deleted lmao
    destroy_tspckt(packet);
  }
}
