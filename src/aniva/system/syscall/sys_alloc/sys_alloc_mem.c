#include "sys_alloc_mem.h"
#include "LibSys/syscall.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <mem/kmem_manager.h>

/*
 * Allocate a range of user pages 
 * TODO: check the user buffer
 */
uint32_t sys_alloc_pages(size_t size, uint32_t flags, void* __user* buffer)
{
  void* base;
  proc_t* current_process;

  if (!size || !buffer || !*buffer)
    return SYS_INV;

  /* Align up the size to the closest next page base */
  size = ALIGN_UP(size, SMALL_PAGE_SIZE);

  current_process = get_current_proc();

  if (!current_process)
    return SYS_ERR;

  /* TODO: Must calls in syscalls that fail may kill the process with the internal error flags set */
  base = (void*)Must(__kmem_alloc_range(current_process->m_root_pd.m_root, current_process, 0, size, KMEM_CUSTOMFLAG_CREATE_USER, KMEM_FLAG_WRITABLE));

  /* Set the user buffer */
  *buffer = base;

  return SYS_OK;
}
