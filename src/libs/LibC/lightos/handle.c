#include "handle.h"
#include "lightos/syscall.h"
#include "lightos/system.h"
#include <stdio.h>

/*
 * Use the value of the handle to detirmine if it might be 
 * invalide, otherwise use a syscall (TODO) to verify if the 
 * handle gives a valid result
 */
BOOL
handle_verify(handle_t handle)
{
  if (handle < 0)
    return FALSE;

  /* TODO: syscall for verification */

  return TRUE;
}

BOOL
get_handle_type(HANDLE handle, HANDLE_TYPE* type)
{
  char t = syscall_1(SYSID_GET_HNDL_TYPE, handle);

  if (t < 0)
    return FALSE;

  /* NOTE: HANDLE_TYPE is unsigned */
  *type = t;

  return TRUE;
}

BOOL
close_handle(HANDLE handle)
{
  QWORD result = syscall_1(SYSID_CLOSE, handle);

  if (result == SYS_OK)
    return TRUE;

  return FALSE;
}

/*
 * Send a syscall to open a handle
 */
HANDLE
open_handle(const char* path, HANDLE_TYPE type, DWORD flags, DWORD mode)
{
  if (!path)
    path = "\0";

  return syscall_4(SYSID_OPEN, (uint64_t)path, type, flags, mode);
}

BOOL 
handle_set_offset(HANDLE handle, QWORD offset)
{
  QWORD result;

  if (!handle_verify(handle))
    return FALSE;

  result = syscall_3(SYSID_SEEK, handle, offset, SEEK_SET);

  if (result != SYS_OK)
    return FALSE;

  return TRUE;
}

BOOL 
handle_get_offset(HANDLE handle, QWORD* boffset)
{
  QWORD value;

  if (!boffset || !handle_verify(handle))
    return FALSE;

  value = syscall_3(SYSID_SEEK, handle, NULL, SEEK_CUR);

  if (value == SYS_INV)
    return FALSE;

  *boffset = value;
  return TRUE;
}

BOOL 
handle_read(HANDLE handle, QWORD buffer_size, VOID* buffer)
{
  uintptr_t bytes_read;

  /* NOTE: sys read returns the amount of bytes that where successfully read */
  bytes_read = syscall_3(SYSID_READ, handle, (uintptr_t)buffer, buffer_size);

  if (bytes_read != buffer_size)
    return FALSE;

  return TRUE;
}

BOOL 
handle_write(HANDLE handle, QWORD buffer_size, VOID* buffer)
{
  uintptr_t error;

  /* NOTE: sys write returns a syscall statuscode */
  error = syscall_3(SYSID_WRITE, handle, (uintptr_t)buffer, buffer_size);

  if (error != SYS_OK)
    return FALSE;

  return TRUE;
}
