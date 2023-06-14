#ifndef __ANIVA_PROC__
#define __ANIVA_PROC__

#include "kevent/kevent.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/base_allocator.h"
#include "mem/page_dir.h"
#include "mem/pg.h"
#include "sync/atomic_ptr.h"

typedef int proc_id;

struct thread;

typedef struct proc {
  char m_name[32];
  proc_id m_id;

  uint32_t m_flags;

  page_dir_t m_root_pd;

  // maps?
  list_t* m_threads;

  // generic_heap_t* m_heap;

  struct thread* m_init_thread;
  struct thread* m_idle_thread;
  struct thread* m_prev_thread;

  size_t m_ticks_used;
  size_t m_requested_max_threads;

  atomic_ptr_t* m_thread_count;

  // bool m_prevent_scheduling;
} proc_t;

/* We will probably find more usages for this */
#define PROC_IDLE           (0x00000001)
#define PROC_KERNEL         (0x00000002) /* This process runs directoy in the kernel */
#define PROC_DRIVER         (0x00000004) /* This process runs with kernel privileges in its own pagemap */
#define PROC_STALLED        (0x00000008)
#define PROC_UNRUNNED       (0x00000010) /* V-card still intact*/
#define PROC_FINISHED       (0x00000020) /* Process should be cleaned up by the scheduler (TODO: let cleaning be done by a reaper thread/proc)*/
#define PROC_DEFERED_HEAP   (0x00000040) /* Wait with creating a heap */
#define PROC_REAPER         (0x00000080) /* Process capable of killing other processes and threads */

proc_t* create_proc(char name[32], FuncPtr entry, uintptr_t args, uint32_t flags);
proc_t* create_kernel_proc(FuncPtr entry, uintptr_t args);
proc_t* create_proc_from_path(const char* path);

/*
 * Murder a proc object with all its threads as well.
 * TODO: we need to verify that these cleanups happen
 * gracefully, meaning that there is no leftover 
 * debris from killing all those threads. 
 * We really want threads to have their own heaps, so that 
 * we don't have to keep track of what they allocated...
 */
void destroy_proc(proc_t*);

ErrorOrPtr proc_add_thread(proc_t* proc, struct thread* thread);
void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args);

ErrorOrPtr try_terminate_process(proc_t* proc);

/* Heh? */
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
