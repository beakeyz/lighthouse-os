#include "sys_open.h"
#include "dev/debug/serial.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "libk/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

HANDLE_t sys_open(const char* __user path, HANDLE_TYPE_t type, uint32_t flags, uint32_t mode)
{
  HANDLE_t ret;
  khandle_t handle;
  ErrorOrPtr result;
  proc_t* process;

  /* TODO: verify user input */

  process = get_current_proc();
  ret = HNDL_INVAL;

  switch (type) {
    case HNDL_TYPE_FILE:
      {
        vobj_t* obj = vfs_resolve(path);

        if (!obj)
          return HNDL_INVAL;

        create_khandle(&handle, &type, obj);

        break;
      }
    case HNDL_TYPE_PROC:
      {
        proc_t* proc = find_proc(path);

        if (!proc)
          return HNDL_NOT_FOUND;

        /* Pretend we didn't find this one xD */
        if (proc->m_flags & PROC_KERNEL)
          return HNDL_NOT_FOUND;

        /* We do allow handles to drivers */
        create_khandle(&handle, &type, proc);

        break;
      }
    case HNDL_TYPE_DRIVER:
    case HNDL_TYPE_FS_ROOT:
    case HNDL_TYPE_KOBJ:
    case HNDL_TYPE_VOBJ:
    case HNDL_TYPE_NONE:
      kernel_panic("Tried to open unimplemented handle!");
      break;
  }

  result = bind_khandle(&process->m_handle_map, &handle);

  if (!IsError(result))
    ret = Release(result);

  return ret;
}

HANDLE_t sys_open_file(const char* __user path, uint32_t flags, uint32_t mode)
{
  kernel_panic("Tried to open proc!");
  return HNDL_INVAL;
}

HANDLE_t sys_open_proc(const char* __user name, uint32_t flags, uint32_t mode)
{
  kernel_panic("Tried to open driver!");
  return HNDL_INVAL;
}

HANDLE_t sys_open_driver(const char* __user name, uint32_t flags, uint32_t mode)
{
  kernel_panic("Tried to open file!");
  return HNDL_INVAL;
}
