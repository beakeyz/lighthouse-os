#include "proc.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include <libk/string.h>
#include "libk/stddef.h"
#include "mem/kmalloc.h"
#include "mem/kmem_manager.h"
#include "proc/context.h"
#include "proc/thread.h"

LIGHT_STATUS create_kernel_proc (proc_t* proc, FuncPtr entry) {
  proc = kmalloc(sizeof(proc_t));
  proc->m_threads = init_list();

  proc->m_id = 0;
  const char* proc_name = "[LIGHTHOUSE]";
  memcpy(proc->m_name, proc_name, strlen(proc_name) + 1);

  thread_t* t = nullptr;
  create_thread(t, entry, "[LazyLight]", true);

  list_append(proc->m_threads, t);

  proc->m_ticks_used = 0;

  return LIGHT_SUCCESS;
}
