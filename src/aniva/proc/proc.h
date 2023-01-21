#ifndef __ANIVA_PROC__
#define __ANIVA_PROC__

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
  size_t m_requested_max_threads;

  bool m_prevent_scheduling;
} proc_t;

proc_t* create_proc(char name[32], proc_id id, FuncPtr entry, uintptr_t args);
proc_t* create_kernel_proc(FuncPtr entry);

#endif // !__ANIVA_PROC__
