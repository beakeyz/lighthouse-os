#include "drv.h"
#include "LibSys/handle.h"
#include "LibSys/handle_def.h"
#include "LibSys/syscall.h"
#include "LibSys/system.h"
#include "sys/types.h"

BOOL 
open_driver(const char* name, DWORD flags, DWORD mode, HANDLE* handle)
{
  BOOL result;
  HANDLE ret;
  HANDLE_TYPE type = HNDL_TYPE_DRIVER;

  if (!handle || !name || !name[0])
    return FALSE;

  ret = open_handle(name, type, flags, mode);

  if (ret == HNDL_INVAL)
    goto fail_exit;

  result = get_handle_type(ret, &type);

  if (!result) {
    close_handle(ret);
    goto fail_exit;
  }

  *handle = ret;

  return TRUE;

fail_exit:
  *handle = HNDL_INVAL;
  return FALSE;
}

BOOL
driver_send_msg(HANDLE handle, DWORD code, VOID* buffer, size_t size)
{
  BOOL ret;
  QWORD sys_status;
  HANDLE_TYPE type = HNDL_TYPE_NONE;

  /* Can't give either a size but or a buffer */
  if ((!buffer && size) || (buffer && !size))
    return FALSE;

  ret = get_handle_type(handle, &type);

  if (!ret || type != HNDL_TYPE_DRIVER)
    return FALSE;

  sys_status = syscall_4(SYSID_SEND_MSG, handle, code, (uintptr_t)buffer, size);

  if (sys_status != SYS_OK)
    return FALSE;

  return TRUE;
}

BOOL 
load_driver(const char* path, DWORD flags, HANDLE* handle)
{
  return FALSE;
}

BOOL 
unload_driver(HANDLE handle)
{
  return FALSE;
}

BOOL 
driver_query_info(HANDLE handle, drv_info_t* info_buffer)
{
  return FALSE;
}
