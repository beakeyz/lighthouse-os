#include "sys_dev.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "mem/kmem_manager.h"
#include "oss/obj.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <dev/device.h>
#include <proc/profile/profile.h>

uintptr_t sys_get_devinfo(HANDLE handle, DEVINFO* binfo)
{
  proc_t* c_proc;
  khandle_t* khandle;
  device_t* device;

  c_proc = get_current_proc();

  /* Check if we're not getting trolled */
  if (IsError(kmem_validate_ptr(c_proc, (vaddr_t)binfo, sizeof(*binfo))))
    return SYS_INV;

  /* Check if we're not getting trolled v2 */
  khandle = find_khandle(&c_proc->m_handle_map, handle);

  if (!khandle || khandle->type != HNDL_TYPE_DEVICE)
    return SYS_INV;

  device = khandle->reference.device;

  /* Check validity (even though the process should never get a hold of a 
   * handle to a device when it does not have the privilege)
   */
  if (!device || !oss_obj_can_proc_access(device->obj, c_proc))
    return SYS_INV;

  /* Get device info */
  if (!KERR_OK(device_getinfo(device, binfo)))
    return SYS_INV;

  return SYS_OK;
}
