#include "sys_open.h"
#include "LibSys/handle_def.h"
#include "LibSys/syscall.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/manifest.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "proc/profile/profile.h"
#include "sched/scheduler.h"

HANDLE sys_open(const char* __user path, HANDLE_TYPE type, uint16_t flags, uint32_t mode)
{
  HANDLE ret;
  khandle_t handle = { 0 };
  ErrorOrPtr result;
  proc_t* current_process;

  if (!path)
    return HNDL_INVAL;

  current_process = get_current_proc();

  if (!current_process || IsError(kmem_validate_ptr(current_process, (uintptr_t)path, 1)))
    return HNDL_INVAL;

  ret = HNDL_NOT_FOUND;

  switch (type) {
    case HNDL_TYPE_FILE:
      {
        vobj_t* obj = vfs_resolve(path);

        if (!obj || !obj->m_child)
          return HNDL_INVAL;

        if (obj->m_type != VOBJ_TYPE_FILE)
          return HNDL_INVAL;

        create_khandle(&handle, &type, obj->m_child);

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
      {
        dev_manifest_t* driver = get_driver(path);

        if (!driver)
          return HNDL_NOT_FOUND;

        create_khandle(&handle, &type, driver);
        break;
      }
    case HNDL_TYPE_PROFILE:
      {
        switch (mode) {
          case HNDL_MODE_CURRENT_PROFILE:
            {
              proc_t* proc = find_proc(path);

              if (!proc)
                return HNDL_NOT_FOUND;

              create_khandle(&handle, &type, proc->m_profile);
              break;
            }
          case HNDL_MODE_SCAN_PROFILE:
            {
              int error;
              proc_profile_t* profile = nullptr;

              error = profile_find(path, &profile); 

              if (error || !profile)
                return HNDL_NOT_FOUND;

              create_khandle(&handle, &type, profile);
              break;
            }
          default:
            break;
        }

        break;
      }
    case HNDL_TYPE_FS_ROOT:
    case HNDL_TYPE_KOBJ:
    case HNDL_TYPE_VOBJ:
    case HNDL_TYPE_NONE:
      kernel_panic("Tried to open unimplemented handle!");
      break;
  }

  if (!handle.reference.kobj)
    return HNDL_NOT_FOUND;

  /* TODO: check for permissions and open with the appropriate flags */

  /* Clear state bits */
  handle.flags = flags;
  handle.flags &= ~HNDL_OPT_MASK;

  /* Copy the handle into the map */
  result = bind_khandle(&current_process->m_handle_map, &handle);

  if (!IsError(result))
    ret = Release(result);

  return ret;
}

HANDLE sys_open_file(const char* __user path, uint16_t flags, uint32_t mode)
{
  kernel_panic("Tried to open proc!");
  return HNDL_INVAL;
}

HANDLE sys_open_proc(const char* __user name, uint16_t flags, uint32_t mode)
{
  kernel_panic("Tried to open driver!");
  return HNDL_INVAL;
}

HANDLE sys_open_driver(const char* __user name, uint16_t flags, uint32_t mode)
{
  kernel_panic("Tried to open file!");
  return HNDL_INVAL;
}

uintptr_t sys_close(HANDLE handle)
{
  khandle_t* c_handle;
  ErrorOrPtr result;
  proc_t* current_process;

  current_process = get_current_proc();

  c_handle = find_khandle(&current_process->m_handle_map, handle);

  if (!c_handle)
    return SYS_INV;

  /* Destroying the khandle */
  result = unbind_khandle(&current_process->m_handle_map, c_handle);

  if (IsError(result))
    return SYS_ERR;
  
  return SYS_OK;
}
