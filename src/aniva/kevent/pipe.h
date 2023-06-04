#ifndef __ANIVA_KEVENT_PIPE__
#define __ANIVA_KEVENT_PIPE__

#include "kevent/context.h"

struct kevent;

typedef struct kevent_pipe_entry {

  void* (*f_pipeline_func)(void*);
  
  struct kevent_pipe_entry* m_next;

} kevent_pipe_entry_t;

typedef struct kevent_pipeline {

  /* The value of the pointer may never change, what it point to may */
  kevent_contex_t m_context;

  kevent_pipe_entry_t* m_head;

} kevent_pipeline_t;

kevent_pipeline_t create_kevent_pipeline(struct kevent* event);

#endif // !__ANIVA_KEVENT_PIPE__
