#include "async_ptr.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "mem/heap.h"
#include "proc/core.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"

async_ptr_t* create_async_ptr(void* volatile* response_buffer, uintptr_t responder_port) {
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
  ret->m_response = (void**)response_buffer;
  ret->m_status = ASYNC_PTR_STATUS_WAITING;

  return ret;
}

void destroy_async_ptr(async_ptr_t* ptr) {
  destroy_mutex(ptr->m_mutex);
  kfree(ptr);
}

void* await(async_ptr_t* ptr) {
  ASSERT_MSG(ptr->m_responder != get_current_scheduling_thread(), "Can't wait on an async ptr on the same thread!");

  mutex_lock(ptr->m_mutex);

  ptr->m_waiter = get_current_scheduling_thread();

  while(!*ptr->m_response);

  mutex_unlock(ptr->m_mutex);

  return *ptr->m_response;
}
