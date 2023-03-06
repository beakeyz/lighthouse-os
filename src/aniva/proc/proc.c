#include "proc.h"
#include <mem/heap.h>
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "interupts/interupts.h"
#include "thread.h"
#include "libk/io.h"
#include <libk/string.h>
#include "core.h"
#include <mem/heap.h>


void generic_proc_idle () {
  println("Entered generic_proc_idle");

  for (;;){}
}

proc_t* create_clean_proc(char name[32], proc_id id) {
  proc_t *proc = kmalloc(sizeof(proc_t));

  if (proc == nullptr) {
    return nullptr;
  }

  proc->m_id = id;
  proc->m_root_pd = nullptr;
  proc->m_idle_thread = create_thread_for_proc(proc, generic_proc_idle, NULL, "idle");
  proc->m_threads = init_list();

  strcpy(proc->m_name, name);

  if (id != 0) {
    proc->m_requested_max_threads = 2;
    proc->m_prevent_scheduling = true;
  } else {
    proc->m_prevent_scheduling = false;
    proc->m_requested_max_threads = PROC_DEFAULT_MAX_THREADS;
  }

  return proc;
}

proc_t* create_proc(char name[32], proc_id id, FuncPtr entry, uintptr_t args) {
  proc_t *proc = create_clean_proc(name, id);

  thread_t* thread = create_thread_for_proc(proc, entry, args, "entry");
  list_append(proc->m_threads, thread);

  return proc;
}

proc_t* create_kernel_proc (FuncPtr entry, uintptr_t  args) {
  return create_proc(PROC_CORE_PROCESS_NAME, 0, entry, args);
}

void proc_add_thread(proc_t* proc, struct thread* thread) {
  if (thread && proc) {
    list_append(proc->m_threads, thread);
  }
}

void proc_add_async_task_thread(proc_t *proc, FuncPtr entry, uintptr_t args) {
  // TODO: generate new unique name
  list_append(proc->m_threads, create_thread_for_proc(proc, entry, args, "AsyncThread #TODO"));
}
