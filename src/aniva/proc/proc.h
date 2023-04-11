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

  struct thread* m_init_thread;
  struct thread* m_idle_thread;
  struct thread* m_prev_thread;

  size_t m_ticks_used;
  size_t m_requested_max_threads;

  // bool m_prevent_scheduling;
} proc_t;

/* We will probably find more usages for this */
#define PROC_IDLE       (0x00000001)
#define PROC_KERNEL     (0x00000002)
#define PROC_STALLED    (0x00000004)
#define PROC_UNRUNNED   (0x00000008)

proc_t* create_proc(char name[32], FuncPtr entry, uintptr_t args, uint32_t flags);
proc_t* create_kernel_proc(FuncPtr entry, uintptr_t args);
proc_t* create_proc_from_path(const char* path);

ErrorOrPtr proc_add_thread(proc_t* proc, struct thread* thread);
void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args);

ErrorOrPtr try_terminate_process(proc_t* proc);

void terminate_process(proc_t* proc);

/*
 * This means that the process will be removed from the scheduler queue
 * and will only be emplaced back once an external source has requested
 * that is will be
 */
void stall_process(proc_t* proc);

static ALWAYS_INLINE bool is_kernel_proc(proc_t* proc) {
  return (proc->m_id == 0);
}

#endif // !__ANIVA_PROC__
