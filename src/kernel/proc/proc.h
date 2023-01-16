#ifndef __LIGHT_PROC__
#define __LIGHT_PROC__
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "proc/thread.h"

typedef size_t proc_id;

typedef struct proc {
  char m_name[32];
  proc_id m_id;

  list_t* m_threads;

  thread_t* m_idle_thread;
  size_t m_ticks_used;
} proc_t;

proc_t* create_kernel_proc(FuncPtr entry);

#endif // !__LIGHT_PROC__
