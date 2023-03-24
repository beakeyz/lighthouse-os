#ifndef __ANIVA_LIBK_ASYNC_PTR__
#define __ANIVA_LIBK_ASYNC_PTR__

#include "mem/pg.h"
#include "sync/atomic_ptr.h"
#include <libk/reference.h>

struct thread;
struct mutex;

typedef enum async_ptr_status {
  ASYNC_PTR_STATUS_WAITING = 0,
  ASYNC_PTR_STATUS_RECIEVED,
  ASYNC_PTR_STATUS_FAILED
} ASYNC_PTR_STATUS;

typedef struct async_ptr {
  volatile void* volatile* m_response_buffer;

  atomic_ptr_t* m_is_buffer_killed;
  refc_t* m_ref;
  struct mutex* m_mutex;
  struct thread* m_responder;
  struct thread* m_waiter;
  ASYNC_PTR_STATUS m_status;
} async_ptr_t;

async_ptr_t* create_async_ptr(uintptr_t responder_port);

void destroy_async_ptr(async_ptr_t** ptr);

void* await(async_ptr_t* ptr);
void async_ptr_assign(void* ptr);

void async_ptr_discard(void (*destructor)(void*), async_ptr_t** ptr_ptr);

#endif // !__ANIVA_LIBK_ASYNC_PTR__
