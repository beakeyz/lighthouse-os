#include "drv.h"
#include "LibSys/handle.h"
#include "LibSys/handle_def.h"
#include "LibSys/syscall.h"
#include "LibSys/system.h"
#include "sys/types.h"

HANDLE_t 
open_driver(const char* name, DWORD flags, DWORD mode)
{
  BOOL result;
  HANDLE_t ret;
  HANDLE_TYPE_t type = HNDL_TYPE_DRIVER;

  ret = open_handle(name, type, flags, mode);

  if (ret == HNDL_INVAL)
    return ret;

  result = get_handle_type(ret, &type);

  if (!result) {
    close_handle(ret);
    return HNDL_INVAL;
  }

  return ret;
}

BOOL
driver_send_msg(HANDLE_t handle, DWORD code, VOID* buffer, size_t size)
{
  BOOL ret;
  QWORD sys_status;
  HANDLE_TYPE_t type = HNDL_TYPE_NONE;

  /* Can't give either a size but or a buffer */
  if ((!buffer && size) || (buffer && !size))
    return FALSE;

  ret = get_handle_type(handle, &type);

  if (!ret || type != HNDL_TYPE_DRIVER)
    return FALSE;

  sys_status = syscall_4(SYSID_SEND_IO_CTL, handle, code, (uintptr_t)buffer, size);

  if (sys_status != SYS_OK)
    return FALSE;

  return TRUE;
}

