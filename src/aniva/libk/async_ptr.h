#ifndef __ANIVA_LIBK_ASYNC_PTR__
#define __ANIVA_LIBK_ASYNC_PTR__

#include "mem/PagingComplex.h"
#include <libk/reference.h>

struct thread;
struct mutex;

typedef enum async_ptr_status {
  ASYNC_PTR_STATUS_WAITING = 0,
  ASYNC_PTR_STATUS_RECIEVED,
  ASYNC_PTR_STATUS_FAILED
} ASYNC_PTR_STATUS;

typedef struct async_ptr {
  void* volatile* m_response_buffer;

  refc_t* m_ref;
  struct mutex* m_mutex;
  struct thread* m_responder;
  struct thread* m_waiter;
  ASYNC_PTR_STATUS m_status;
} async_ptr_t;

async_ptr_t* create_async_ptr(uintptr_t responder_port);

void destroy_async_ptr(async_ptr_t* ptr);

void* await(async_ptr_t* ptr);
void async_ptr_assign(void* ptr);
void async_ptr_discard(async_ptr_t* ptr);

#endif // !__ANIVA_LIBK_ASYNC_PTR__
