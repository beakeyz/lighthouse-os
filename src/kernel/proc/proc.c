#include "proc.h"
#include "kmain.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include <libk/string.h>
#include "mem/kmalloc.h"
#include "proc/thread.h"

LIGHT_STATUS create_kernel_proc (proc_t* proc) {
  proc = kmalloc(sizeof(proc_t));
  proc->m_threads = init_list();

  thread_t* idle = kmalloc(sizeof(thread_t));
  const char* idle_name = "[LazyLight]";
  memcpy(idle->m_name, idle_name, strlen(idle_name) + 1);
  idle->m_cpu = g_GlobalSystemInfo.m_current_core->m_cpu_num;
  idle->m_parent_proc = proc;
  idle->m_ticks_elapsed = 0;
  idle->m_current_state = RUNNABLE;


  proc->m_id = 0;
  const char* proc_name = "[LIGHTHOUSE]";
  memcpy(proc->m_name, proc_name, strlen(proc_name) + 1);

  proc->m_ticks_used = 0;

  return LIGHT_SUCCESS;
}
