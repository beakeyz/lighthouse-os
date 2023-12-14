#ifndef __ANIVA_KEVENT_PIPE__
#define __ANIVA_KEVENT_PIPE__

#include "kevent/context.h"
#include "libk/flow/error.h"
#include "mem/zalloc.h"

struct kevent;
struct kevent_contex;

typedef struct kevent_pipe_entry {
  /* This is the functin that was yoinked from the khook */
  uint32_t (*f_pipeline_func)(struct kevent_contex);
  /* Pointer to the next pipe entry, which should have equal or lower privilege */
  struct kevent_pipe_entry* m_next;
} kevent_pipe_entry_t;

/*
 * Execution pipeline for a kevent fire
 *
 * This object is almost always allocated on the stack and is always destroyed after the
 * kevent fire is completed
 */
typedef struct kevent_pipeline {
  /* The value of the pointer may never change, what it point to may */
  kevent_contex_t* m_context;

  /* Allocator with which we allocate our entries for this pipeline */
  zone_allocator_t* m_entry_allocator;

  kevent_pipe_entry_t* m_entries;
  kevent_pipe_entry_t* m_exec_head;
} kevent_pipeline_t;

void init_kevent_pipes();

int create_kevent_pipeline(kevent_pipeline_t* out, struct kevent* event, kevent_contex_t* context);
void destroy_kevent_pipeline(kevent_pipeline_t* pipeline);

/*
 * Execute the current head of the pipeline and shift the data forward
 */
int kpipeline_execute_next(kevent_pipeline_t* pipeline);

#endif // !__ANIVA_KEVENT_PIPE__
