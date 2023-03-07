#include "socket.h"
#include <mem/heap.h>
#include <system/asm_specifics.h>
#include "core.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/io.h"
#include "libk/queue.h"
#include "libk/string.h"
#include "proc/default_socket_routines.h"
#include "sched/scheduler.h"
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
ErrorOrPtr send_packet_to_socket(uint32_t port, void* buffer, size_t buffer_size) {
  CHECK_AND_DO_DISABLE_INTERRUPTS();
  // TODO: list lookup
  threaded_socket_t *socket = find_registered_socket(port);

  if (socket == nullptr || socket->m_parent == nullptr) {
    return Error();
  }

  tspckt_t *packet = create_tspckt(buffer, buffer_size);

  // don't allow buffer restriction violations
  if (packet->m_packet_size > socket->m_max_size_per_buffer) {
    destroy_tspckt(packet);
    return Error();
  }

  queue_enqueue(socket->m_buffers, packet);

  CHECK_AND_TRY_ENABLE_INTERRUPTS();
  return Success((uintptr_t)&packet->m_response);
}

tspckt_t *send_packet_to_socket_blocking(uint32_t port, void* buffer, size_t buffer_size) {

  ErrorOrPtr result = send_packet_to_socket(port, buffer, buffer_size);
  if (result.m_status == ANIVA_FAIL) {
    return nullptr;
  }

  tspckt_t **packet_dptr = (tspckt_t**) Release(result);

  for (;;) {
    tspckt_t *response_ptr = (tspckt_t*)*packet_dptr;
    if (response_ptr != nullptr) {
      return response_ptr;
    }
  }
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
  
  tspckt_t* packet;
  while (validate_tspckt(packet = queue_dequeue(buffer))) {

    //thread_set_state(thread, RUNNING);
    size_t data_size = packet->m_packet_size - sizeof(tspckt_t);

    if (data_size == sizeof(uint16_t)) {
      uint16_t data = *(uint16_t*)packet->m_data;
      uint8_t ident;
      uint8_t routine;

      ANIVA_STATUS result = parse_socket_routing(data, &ident, &routine);

      if (result == ANIVA_FAIL) {
        goto skip_callback;
      }

      if (ident == SOCKET_ROUTINE_IDENT) {
        // TODO:
        switch (routine) {
          case SOCKET_ROUTINE_DEBUG:
            println("Recieved SOCKET_ROUTINE_DEBUG routine!");
            break;
          case SOCKET_ROUTINE_IGNORE_NEXT:
            break;
          case SOCKET_ROUTINE_EXIT:
            socket_set_flag(thread->m_socket, TS_SHOULD_EXIT, true);
            break;
        }
      }
      goto skip_callback;
    }

    thread->m_socket->m_on_packet(packet->m_data, data_size);
    // we jump here when we are done handeling a potential socketroutine
  skip_callback:
    destroy_tspckt(packet);
  }
}
