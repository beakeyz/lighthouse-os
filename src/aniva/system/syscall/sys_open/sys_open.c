#include "sys_open.h"
#include "lightos/handle_def.h"
#include "lightos/proc/var_types.h"
#include "lightos/syscall.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/device.h"
#include "dev/manifest.h"
#include "fs/file.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/vobj.h"
#include "kevent/event.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "proc/profile/profile.h"
#include "proc/profile/variable.h"
#include "sched/scheduler.h"

HANDLE sys_open(const char* __user path, HANDLE_TYPE type, uint32_t flags, uint32_t mode)
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

  /*
   * TODO: check for permissions and open with the appropriate flags 
   *
   * We want to do this by checking the profile that this process is in and checking if processes 
   * in that profile have access to this resource
   */

  switch (type) {
    case HNDL_TYPE_FILE:
      {
        file_t* file;
        vobj_t* obj = vfs_resolve(path);

        if (!obj)
          return HNDL_NOT_FOUND;

        file = vobj_get_file(obj);

        if (!file)
          return HNDL_INVAL;

        init_khandle(&handle, &type, file);
        break;
      }
    case HNDL_TYPE_DEVICE:
      {
        device_t* device;
        vobj_t* obj = vfs_resolve(path);

        if (!obj)
          return HNDL_NOT_FOUND;

        device = vobj_get_device(obj);

        if (!device)
          return HNDL_INVAL;

        init_khandle(&handle, &type, device);
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
        init_khandle(&handle, &type, proc);

        break;
      }
    case HNDL_TYPE_DRIVER:
      {
        dev_manifest_t* driver = get_driver(path);

        if (!driver)
          return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, driver);
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

              init_khandle(&handle, &type, proc->m_profile);
              break;
            }
          case HNDL_MODE_SCAN_PROFILE:
            {
              int error;
              proc_profile_t* profile = nullptr;

              error = profile_find(path, &profile); 

              if (error || !profile)
                return HNDL_NOT_FOUND;

              init_khandle(&handle, &type, profile);
              break;
            }
          default:
            break;
        }

        break;
      }
    case HNDL_TYPE_EVENT:
      {
        struct kevent* event;

        event = kevent_get(path);

        if (!event)
          return HNDL_NOT_FOUND;

        init_khandle(&handle, &type, event);
        break;
      }
    case HNDL_TYPE_EVENTHOOK:
      break;
    case HNDL_TYPE_FS_ROOT:
    case HNDL_TYPE_KOBJ:
    case HNDL_TYPE_VOBJ:
    case HNDL_TYPE_NONE:
      kernel_panic("Tried to open unimplemented handle!");
      break;
  }

  if (!handle.reference.kobj)
    return HNDL_NOT_FOUND;

  /* Clear state bits */
  khandle_set_flags(&handle, flags);

  /* Copy the handle into the map */
  result = bind_khandle(&current_process->m_handle_map, &handle);

  if (!IsError(result))
    ret = Release(result);

  return ret;
}

/*!
 * @brief Open a handle to a profile variable from a certain profile handle
 *
 * We don't check for any permissions here, since anyone may recieve a handle to any profile or any profile handle
 * (except if its hidden or something else prevents the opening of pvars)
 */
HANDLE sys_open_pvar(const char* __user name, HANDLE profile_handle, uint16_t flags)
{
  int error;
  ErrorOrPtr result;
  proc_t* current_proc;
  profile_var_t* pvar;
  proc_profile_t* current_profile;
  proc_profile_t* target_profile;
  khandle_t* profile_khndl;
  khandle_t pvar_khndl;
  khandle_type_t type = HNDL_TYPE_PVAR;

  current_proc = get_current_proc();

  /* Validate pointers */
  if (!current_proc || IsError(kmem_validate_ptr(current_proc, (uint64_t)name, 1)))
    return HNDL_INVAL;

  /* Grab the kernel handle to the target profile */
  profile_khndl = find_khandle(&current_proc->m_handle_map, profile_handle);

  if (!profile_khndl || profile_khndl->type != HNDL_TYPE_PROFILE)
    return HNDL_INVAL;

  /* Extract the actual profile pointers */
  current_profile = current_proc->m_profile;
  target_profile = profile_khndl->reference.profile;

  /* Check if there is an actual profile (non-null is just assume yes =) ) */
  if (!target_profile)
    return HNDL_INVAL;

  /* Grab the profile variable we want */
  pvar = nullptr;
  error = profile_get_var(target_profile, name, &pvar); 

  if (error || !pvar)
    return HNDL_NOT_FOUND;

  /* Spoof a not-found when the permissions don't match =) */
  if (!profile_can_see_var(current_profile, pvar))
    return HNDL_NOT_FOUND;

  /* Create a kernel handle */
  init_khandle(&pvar_khndl, &type, pvar);

  /* Set the flags we want */
  khandle_set_flags(&pvar_khndl, flags);

  /* Copy the handle into the map */
  result = bind_khandle(&current_proc->m_handle_map, &pvar_khndl);

  if (!IsError(result))
    return Release(result);

  return HNDL_INVAL;
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

/*!
 * @brief: Syscall entry for the creation of profile variables
 *
 * NOTE: this syscall kinda assumes that the caller has permission to create variables on this profile if
 * it managed to get a handle with write access. This means that permissions need to be managed correctly
 * by sys_open in that regard. See the TODO comment inside sys_open
 */
bool sys_create_pvar(HANDLE profile_handle, const char* __user name, enum PROFILE_VAR_TYPE type, uint32_t flags, void* __user value)
{
  proc_t* proc;
  khandle_t* khandle;
  proc_profile_t* profile;
  profile_var_t* var;

  proc = get_current_proc();

  if (IsError(kmem_validate_ptr(proc, (uintptr_t)name, 1)))
    return false;

  if (IsError(kmem_validate_ptr(proc, (uintptr_t)value, 1)))
    return false;

  khandle = find_khandle(&proc->m_handle_map, profile_handle);

  /* Invalid handle =/ */
  if (!khandle || khandle->type != HNDL_TYPE_PROFILE)
    return false;

  /* Can't write to this handle =/ */
  if ((khandle->flags & HNDL_FLAG_WRITEACCESS) != HNDL_FLAG_WRITEACCESS)
    return false;

  profile = khandle->reference.profile;

  /* FIXME: we need a way to safely copy strings from userspace into kernel space */
  if (type == PROFILE_VAR_TYPE_STRING)
    /* FIXME: unsafe strdup call (also we just kinda assume there is a string here wtf: NEVER TRUST USERSPACE) */
    value = strdup(value);

  /* FIXME: unsafe strdup call (also we just kinda assume there is a string here wtf: NEVER TRUST USERSPACE) */
  name = strdup(name);

  /* Create a variable */
  var = create_profile_var(name, type, flags, (uint64_t)value);

  if (!var)
    return false;

  /* Can we successfully add this var? */
  if (profile_add_var(profile, var))
    goto release_and_exit;

  return true;

release_and_exit:
  release_profile_var(var);
  return false;
}
