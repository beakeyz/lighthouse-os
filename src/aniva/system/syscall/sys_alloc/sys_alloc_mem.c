#include "sys_alloc_mem.h"
#include "lightos/syscall.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/resource.h"
#include <mem/kmem_manager.h>

/*
 * Allocate a range of user pages 
 * TODO: check the user buffer
 */
uint32_t sys_alloc_pages(size_t size, uint32_t flags, void* __user* buffer)
{
  proc_t* current_process;

  if (!size || !buffer)
    return SYS_INV;

  /* Align up the size to the closest next page base */
  size = ALIGN_UP(size, SMALL_PAGE_SIZE);

  current_process = get_current_proc();

  if (!current_process || IsError(kmem_validate_ptr(current_process, (uintptr_t)buffer, 1)))
    return SYS_INV;

  /* TODO: Must calls in syscalls that fail may kill the process with the internal error flags set */
  *buffer = (void*)Must(kmem_user_alloc_range(current_process, size, NULL, KMEM_FLAG_WRITABLE));

  return SYS_OK;
}
