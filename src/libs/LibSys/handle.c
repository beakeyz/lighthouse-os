#include "handle.h"
#include "LibSys/syscall.h"
#include "LibSys/system.h"

/*
 * Use the value of the handle to detirmine if it might be 
 * invalide, otherwise use a syscall (TODO) to verify if the 
 * handle gives a valid result
 */
BOOL
verify_handle(handle_t handle)
{
  if (handle < 0)
    return FALSE;

  /* TODO: syscall for verification */

  return TRUE;
}

BOOL
get_handle_type(HANDLE_t handle, HANDLE_TYPE_t* type)
{
  char t = syscall_1(SYSID_GET_HNDL_TYPE, handle);

  if (t < 0)
    return FALSE;

  /* NOTE: HANDLE_TYPE_t is unsigned */
  *type = t;

  return TRUE;
}

BOOL
close_handle(HANDLE_t handle)
{
  QWORD result = syscall_1(SYSID_CLOSE, handle);

  if (result == SYS_OK)
    return TRUE;

  return FALSE;
}

/*
 * Send a syscall to open a handle
 */
HANDLE_t
open_handle(const char* path, HANDLE_TYPE_t type, DWORD flags, DWORD mode)
{
  HANDLE_t ret;

  if (!path)
    return HNDL_INVAL;

  ret = syscall_4(SYSID_OPEN, (uint64_t)path, type, flags, mode);

  return ret;
}
