#include "proc.h"
#include "mem/kmalloc.h"
#include "thread.h"
#include <libk/string.h>

void generic_proc_idle () {
  println("Entered generic_proc_idle");
  for (;;) {
    print("h");
  }
}

proc_t* create_proc(char name[32], proc_id id, FuncPtr entry, uintptr_t args) {
  proc_t *proc = kmalloc(sizeof(proc_t));
  proc->m_id = id;
  proc->m_idle_thread = create_thread(generic_proc_idle, NULL, "idle", (id == 0));
  proc->m_threads = init_list();
  list_append(proc->m_threads, create_thread(entry, args, "First thread", (id == 0)));
  thread_prepare_context(proc->m_idle_thread);
  thread_prepare_context((thread_t*) list_get(proc->m_threads, 0));

  strcpy(proc->m_name, name);

  if (id != 0) {
    proc->m_requested_max_threads = 2;
    proc->m_prevent_scheduling = true;
  } else {
    proc->m_prevent_scheduling = false;
    proc->m_requested_max_threads = 52;
  }
}

proc_t* create_kernel_proc (FuncPtr entry) {
  return create_proc("[aniva-core]", 0, entry, NULL);
}
