#include "proc.h"
#include <libk/string.h>

void generic_proc_idle () {
  for (;;) {}
}

proc_t* create_proc(char name[32], proc_id id, FuncPtr entry, uintptr_t args) {
  proc_t *proc = kmalloc(sizeof(proc_t));
  proc->m_id = id;
  proc->m_idle_thread = create_thread(generic_proc_idle, NULL, "idle", (id == 0));
  proc->m_threads = init_list();
  list_append(proc->m_threads, create_thread(entry, args, "First thread", (id == 0)));

  strcpy(proc->m_name, name);

  if (id != 0) {
    proc->m_requested_max_threads = 2;
    proc->m_prevent_scheduling = true;
  } else {
    proc->m_prevent_scheduling = false;
    proc->m_prevent_scheduling = 52;
  }
}

proc_t* create_kernel_proc (FuncPtr entry) {
  return create_proc("[aniva_core]", 0, entry, NULL);
}
