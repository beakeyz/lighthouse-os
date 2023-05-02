#include "proc.h"
#include <mem/heap.h>
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "entry/entry.h"
#include "interupts/interupts.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "sched/scheduler.h"
#include "system/processor/processor.h"
#include "thread.h"
#include "libk/io.h"
#include <libk/string.h>
#include "core.h"
#include <mem/heap.h>

void generic_proc_idle () {
  println("Entered generic_proc_idle");

  for (;;) {
    scheduler_yield();
  }
}

proc_t* create_proc(char name[32], FuncPtr entry, uintptr_t args, uint32_t flags) {
  proc_t *proc = kmalloc(sizeof(proc_t));
  memset(proc, 0, sizeof(proc_t));

  if (proc == nullptr) {
    return nullptr;
  }

  proc->m_id = Must(generate_new_proc_id());
  proc->m_flags = flags | PROC_UNRUNNED;

  // Only create new page dirs for non-kernel procs
  if ((flags & PROC_KERNEL) == 0 || (flags & PROC_DRIVER) == PROC_DRIVER) {
    proc->m_requested_max_threads = 2;
  //  proc->m_prevent_scheduling = true;
    proc->m_root_pd = kmem_create_page_dir(KMEM_CUSTOMFLAG_CREATE_USER, 0);
  } else {
    proc->m_root_pd.m_root = get_current_processor()->m_page_dir;
    proc->m_root_pd.m_kernel_high = (uintptr_t)&_kernel_end;
    proc->m_root_pd.m_kernel_low = (uintptr_t)&_kernel_start;
  //  proc->m_prevent_scheduling = false;
    proc->m_requested_max_threads = PROC_DEFAULT_MAX_THREADS;
  }

  if ((flags & PROC_DEFERED_HEAP) == PROC_DEFERED_HEAP) {
    proc->m_heap = nullptr;
  } else {
    proc->m_heap = create_zone_allocator(128 * Kib, 0)->m_heap;
  }

  proc->m_idle_thread = create_thread_for_proc(proc, generic_proc_idle, NULL, "idle");
  proc->m_threads = init_list();

  memcpy(proc->m_name, name, 31);

  thread_t* thread = create_thread_for_proc(proc, entry, args, "main");
  Must(proc_add_thread(proc, thread));

  return proc;
}

#define IS_KERNEL_FUNC(func) true

proc_t* create_kernel_proc (FuncPtr entry, uintptr_t  args) {

  /* TODO: check */
  if (!IS_KERNEL_FUNC(entry)) {
    return nullptr;
  }

  return create_proc(PROC_CORE_PROCESS_NAME, entry, args, PROC_KERNEL);
}

proc_t* create_proc_from_path(const char* path) {
  kernel_panic("TODO: create_proc_from_path");
  return nullptr;
}

void destroy_proc(proc_t* proc) {

  FOREACH(i, proc->m_threads) {
    /* Kill every thread */
    destroy_thread(i->data);
  }

  /* Free everything else */

  /* 
   * Kill the root pd if it has one, other than the currently active page dir. 
   * We already kinda expect this to only be called from kernel context, but 
   * you never know... For that we simply allow every page directory to be 
   * killed as long as we are not currently using it :clown: 
   */
  if (proc->m_root_pd.m_root != get_current_processor()->m_page_dir) {
    kmem_destroy_page_dir(proc->m_root_pd.m_root);
  }
}

ErrorOrPtr try_terminate_process(proc_t* proc) {

  /* If we try to terminate while the process is marked finished, we can just hard-kill it */
  if (proc->m_flags & PROC_FINISHED) {
    /* We honestly don't really care if its contained in the scheduler at this point */
    ANIVA_STATUS remove_res = sched_remove_proc(proc);

    destroy_proc(proc);

    /* Warning? */
    return Success(0);
  }

  pause_scheduler();

  proc->m_flags |= PROC_FINISHED;

  resume_scheduler();

  return Success(0);
}

void terminate_process(proc_t* proc);

ErrorOrPtr proc_add_thread(proc_t* proc, struct thread* thread) {
  if (!thread || !proc)
    return Error();

  ErrorOrPtr does_contain = list_indexof(proc->m_threads, thread);

  if (does_contain.m_status == ANIVA_SUCCESS) {
    return Error();
  }

  /* If this is the first thread, we need to make sure it gets ran first */
  if (proc->m_threads->m_length == 0 && proc->m_init_thread == nullptr) {
    proc->m_init_thread = thread;
    /* Ensure the schedulers picks up on this fact */
    proc->m_flags |= PROC_UNRUNNED;
  }

  list_append(proc->m_threads, thread);
  return Success(0);
}

void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args) {
  // TODO: generate new unique name
  //list_append(proc->m_threads, create_thread_for_proc(proc, entry, args, "AsyncThread #TODO"));
  kernel_panic("TODO: implement async task threads");
}
