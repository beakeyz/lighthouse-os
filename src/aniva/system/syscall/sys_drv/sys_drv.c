#include "sys_drv.h"
#include "LibSys/handle_def.h"
#include "LibSys/syscall.h"
#include "dev/core.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include <dev/manifest.h>

uintptr_t
sys_send_ioctl(HANDLE_t handle, driver_control_code_t code, void __user* buffer, size_t size)
{
  ErrorOrPtr result;
  khandle_t* c_hndl;
  proc_t* c_proc;

  c_proc = get_current_proc();

  /* Check the buffer(s) if they are given */
  if (buffer && size && IsError(kmem_validate_ptr(c_proc, (vaddr_t)buffer, size)))
    return SYS_INV;

  /* Find the handle */
  c_hndl = find_khandle(&c_proc->m_handle_map, handle);

  if (!c_hndl)
    return SYS_INV;

  result = Success(0);

  switch (c_hndl->type) {
    case KHNDL_TYPE_DRIVER:
      {
        dev_manifest_t* driver = c_hndl->reference.driver;

        /* NOTE: this call locks the manifest */
        result = driver_send_packet(driver->m_url, code, buffer, size);
        break;
      }
    case KHNDL_TYPE_FILE:
      {
        file_t* file = c_hndl->reference.file;

        /* TODO: follow unix? */
        (void)file;
        result = Error();
        break;
      }
    default:
      return SYS_INV;
  }

  if (IsError(result))
    return SYS_ERR;

  return SYS_OK;
}

uintptr_t
sys_get_handle_type(HANDLE_t handle)
{
  khandle_t* c_handle;
  proc_t* c_proc;

  c_proc = get_current_proc();
  c_handle = find_khandle(&c_proc->m_handle_map, handle);

  if (!c_handle)
    return SYS_INV;

  switch(c_handle->type) {
    case KHNDL_TYPE_FILE:
      return HNDL_TYPE_FILE;
    case KHNDL_TYPE_VOBJ:
      return HNDL_TYPE_VOBJ;
    case KHNDL_TYPE_KOBJ:
      return HNDL_TYPE_KOBJ;
    case KHNDL_TYPE_PROC:
      return HNDL_TYPE_PROC;
    case KHNDL_TYPE_DRIVER:
      return HNDL_TYPE_DRIVER;
    case KHNDL_TYPE_THREAD:
      return HNDL_TYPE_THREAD;
    default:
      break;
  }
  return HNDL_TYPE_NONE;
}
