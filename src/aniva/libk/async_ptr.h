#ifndef __ANIVA_LIBK_ASYNC_PTR__
#define __ANIVA_LIBK_ASYNC_PTR__

#include "mem/PagingComplex.h"
#include "proc/thread.h"
#include <sync/mutex.h>

typedef enum async_ptr_status {
  ASYNC_PTR_STATUS_WAITING = 0,
  ASYNC_PTR_STATUS_RECIEVED,
  ASYNC_PTR_STATUS_FAILED
} ASYNC_PTR_STATUS;

typedef struct async_ptr {
  void* volatile* m_response;

  mutex_t* m_mutex;
  thread_t* m_responder;
  thread_t* m_waiter;
  ASYNC_PTR_STATUS m_status;
} async_ptr_t;

async_ptr_t* create_async_ptr(void* volatile* response_buffer, uintptr_t responder_port);
void destroy_async_ptr(async_ptr_t* ptr);

void* await(async_ptr_t* ptr);

#endif // !__ANIVA_LIBK_ASYNC_PTR__
