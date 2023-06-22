#include "handle.h"
#include "LibSys/syscall.h"
#include "LibSys/system.h"

/*
 * Use the value of the handle to detirmine if it might be 
 * invalide, otherwise use a syscall (TODO) to verify if the 
 * handle gives a valid result
 */
BOOL verify_handle(
  __IN__ handle_t handle
) {
  if (handle < 0)
    return FALSE;

  /* TODO: syscall for verification */

  return TRUE;
}

/*
 * Send a syscall to open a handle
 */
HANDLE_t open_handle(
  __IN__ const char* path,
  __IN__ __OPTIONAL__ HANDLE_TYPE_t type,
  __IN__ DWORD flags,
  __IN__ DWORD mode
) {
  HANDLE_t ret;

  if (!path)
    return HNDL_INVAL;

  ret = syscall_4(SYSID_OPEN, (uintptr_t)path, type, flags, mode);

  return ret;
}
