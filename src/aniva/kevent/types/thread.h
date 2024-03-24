#ifndef __ANIVA_KEVENT_THREAD_EVENT__
#define __ANIVA_KEVENT_THREAD_EVENT__

#include <libk/stddef.h>

struct proc;

enum KEVENT_THREAD_EVENTTYPE {
  /* Process creation */
  THREAD_EVENTTYPE_CREATE,
  /* Process destruction */
  THREAD_EVENTTYPE_DESTROY,
  /* Process rescheduling to another CPU */
  THREAD_EVENTTYPE_RESCHEDULE
};

typedef struct kevent_thread_ctx {
  struct thread* thread;

  enum KEVENT_THREAD_EVENTTYPE type;

  uint32_t old_cpu_id;
  uint32_t new_cpu_id;
} kevent_thread_ctx_t;

#endif // !__ANIVA_KEVENT_PROC_EVENT__
