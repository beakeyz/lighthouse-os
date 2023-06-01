#include "async_ptr.h"
#include "dev/debug/serial.h"
#include "interrupts/interrupts.h"
#include "libk/error.h"
#include "libk/reference.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "proc/core.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"

async_ptr_t* create_async_ptr(uintptr_t responder_port) {
  async_ptr_t* ret = kmalloc(sizeof(async_ptr_t));

  threaded_socket_t* responder = find_registered_socket(responder_port);

  if (responder == nullptr) {
    kfree(ret);
    return nullptr;
  }

  ASSERT_MSG(thread_is_socket(responder->m_parent), "Parent thread is not a socket!");

  ret->m_mutex = create_mutex(0);
  ret->m_responder = responder->m_parent;
  ret->m_waiter = nullptr;
  ret->m_is_buffer_killed = create_atomic_ptr_with_value(false);
  ret->m_ref = create_refc(destroy_async_ptr, ret);

  // we prepare our own buffer, which can be filled from an external source
  ret->m_response_buffer = kmalloc(sizeof(void*));
  // zero dat bitch
  memset((void*)ret->m_response_buffer, 0x00, sizeof(void*));

  ret->m_status = ASYNC_PTR_STATUS_WAITING;


  return ret;
}

void destroy_async_ptr(async_ptr_t** ptr_p) {
  async_ptr_t* ptr = *ptr_p;
  if (!ptr || ptr->m_ref->m_count > 0)
    return;
  
  if (!atomic_ptr_load(ptr->m_is_buffer_killed)) {
    kfree((void*)ptr->m_response_buffer);
  }
  destroy_atomic_ptr(ptr->m_is_buffer_killed);
  destroy_mutex(ptr->m_mutex);
  destroy_refc(ptr->m_ref);
  memset(ptr, 0, sizeof(*ptr));
  kfree(ptr);
  println(get_current_scheduling_thread()->m_name);
  println("DESTROYING ASYNC PTR");
  *ptr_p = 0;
}

void* await(async_ptr_t* ptr) {

  if (!ptr)
    return nullptr;

  ASSERT_MSG(ptr->m_responder != get_current_scheduling_thread(), "Can't wait on an async ptr on the same thread!");

  mutex_lock(ptr->m_mutex);

  // FIXME: this ref/unref dance can be simplefied into
  // the assumption that after awaiting an async pointer,
  // you don't need it anymore. We need to verify that 
  // this works before removing this though, because having
  // is like this does mean that other threads can reference this
  // and then prevent destruction after this thread is done waiting.
  // the question is: do we want that to be possible?
  
  ref(ptr->m_ref);
  ptr->m_waiter = get_current_scheduling_thread();

  while(ptr->m_response_buffer && !*ptr->m_response_buffer) {
    println(get_current_scheduling_thread()->m_name);
    scheduler_yield();
    if (atomic_ptr_load(ptr->m_is_buffer_killed) != false) {
      kfree((void*)ptr->m_response_buffer);
      ptr->m_response_buffer = nullptr;
      break;
    }
  }

  mutex_unlock(ptr->m_mutex);

  void* response = nullptr; 
  if (ptr->m_response_buffer) {
    response = (void*)*ptr->m_response_buffer;
  }

  unref(ptr->m_ref);

  return response;
}

void async_ptr_assign(void* ptr) {
  // TODO
  kernel_panic("UNIMPLEMENTED");
}

void async_ptr_discard(void (*destructor)(void*), async_ptr_t** ptr_ptr) {
  async_ptr_t* ptr = *ptr_ptr;
  if (!ptr)
    return;

  // take the mutex
  mutex_lock(ptr->m_mutex);

  if (*ptr->m_response_buffer && atomic_ptr_load(ptr->m_is_buffer_killed) == false)
    destructor((void*)*ptr->m_response_buffer);

  destroy_async_ptr(ptr_ptr);
}
