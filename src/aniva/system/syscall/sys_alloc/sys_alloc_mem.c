#include "sys_alloc_mem.h"
#include "LibSys/syscall.h"
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
  void* base;
  uint64_t first_usable_base;
  proc_t* current_process;

  if (!size || !buffer)
    return SYS_INV;

  /* Align up the size to the closest next page base */
  size = ALIGN_UP(size, SMALL_PAGE_SIZE);

  current_process = get_current_proc();

  if (!current_process || IsError(kmem_validate_ptr(current_process, (uintptr_t)buffer, 1)))
    return SYS_INV;

  /* FIXME: this is not very safe, we need to randomize the start of process data probably lmaoo */
  first_usable_base = Must(resource_find_usable_range(current_process->m_resource_bundle, KRES_TYPE_MEM, size));

  /* TODO: Must calls in syscalls that fail may kill the process with the internal error flags set */
  base = (void*)Must(__kmem_map_and_alloc_scattered(
        current_process->m_root_pd.m_root,
        current_process->m_resource_bundle,
        first_usable_base,
        size,
        KMEM_CUSTOMFLAG_CREATE_USER,
        KMEM_FLAG_WRITABLE
        ));

  /* Set the user buffer */
  *buffer = base;

  return SYS_OK;
}
