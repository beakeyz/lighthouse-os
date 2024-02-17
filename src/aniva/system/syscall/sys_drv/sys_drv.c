#include "sys_drv.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "dev/core.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "proc/profile/variable.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include <dev/manifest.h>

#include <lightos/driver/ctl.h>

uintptr_t
sys_send_message(HANDLE handle, driver_control_code_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
  ErrorOrPtr result;
  khandle_t* c_hndl;
  proc_t* c_proc;

  c_proc = get_current_proc();

  /* Check the buffer(s) if they are given */
  if (buffer && IsError(kmem_validate_ptr(c_proc, (vaddr_t)buffer, size)))
    return SYS_INV;

  if (out_buffer && IsError(kmem_validate_ptr(c_proc, (vaddr_t)out_buffer, size)))
    return SYS_INV;

  /* Find the handle */
  c_hndl = find_khandle(&c_proc->m_handle_map, handle);

  if (!c_hndl)
    return SYS_INV;

  result = Success(0);

  switch (c_hndl->type) {
    case HNDL_TYPE_DRIVER:
      {
        drv_manifest_t* driver = c_hndl->reference.driver;

        /* NOTE: this call locks the manifest */
        result = driver_send_msg_ex(driver, code, buffer, size, out_buffer, out_size);
        break;
      }
    case HNDL_TYPE_FILE:
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
sys_send_drv_ctl(HANDLE handle, enum DRV_CTL_MODE mode)
{
  kernel_panic("TODO: sys_send_drv_ctl");
  return SYS_OK;
}

uintptr_t
sys_get_handle_type(HANDLE handle)
{
  khandle_t* c_handle;
  proc_t* c_proc;

  c_proc = get_current_proc();
  c_handle = find_khandle(&c_proc->m_handle_map, handle);

  if (!c_handle)
    return HNDL_TYPE_NONE;

  return c_handle->type;
}

bool 
sys_get_pvar_type(HANDLE pvar_handle, enum PROFILE_VAR_TYPE __user* type_buffer)
{
  profile_var_t* var;
  khandle_t* handle;
  proc_t* current_proc;

  current_proc = get_current_proc();

  /* Check the buffer address */
  if (IsError(kmem_validate_ptr(current_proc, (uint64_t)type_buffer, sizeof(type_buffer))))
    return false;

  /* Find the khandle */
  handle = find_khandle(&current_proc->m_handle_map, pvar_handle);

  if (handle->type != HNDL_TYPE_PVAR)
    return false;

  /* Extract the profile variable */
  var = handle->reference.pvar;

  if (!var)
    return false;

  /*
   * Set the userspace buffer 
   * TODO: create a kmem routine that handles memory opperations to and from
   * userspace
   */
  *type_buffer = var->type;
  return true;
}
