#ifndef __ANIVA_PROC__
#define __ANIVA_PROC__

#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/pg.h"

typedef int proc_id;

struct thread;

typedef struct proc {
  char m_name[32];
  proc_id m_id;

  uint32_t m_flags;

  pml_entry_t* m_root_pd;

  // maps?
  list_t* m_threads;
  list_t* m_runnable_threads;

  struct thread* m_idle_thread;
  struct thread* m_prev_thread;

  size_t m_ticks_used;
  size_t m_requested_max_threads;

  bool m_prevent_scheduling;
} proc_t;

/* We will probably find more useages for this */
#define PROC_IDLE   (0x00000001)

proc_t* create_clean_proc(char name[32], proc_id id);
proc_t* create_proc(char name[32], proc_id id, FuncPtr entry, uintptr_t args);
proc_t* create_kernel_proc(FuncPtr entry, uintptr_t args);

ErrorOrPtr proc_add_thread(proc_t* proc, struct thread* thread);
void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args);

static ALWAYS_INLINE bool is_kernel_proc(proc_t* proc) {
  return (proc->m_id == 0);
}

#endif // !__ANIVA_PROC__
