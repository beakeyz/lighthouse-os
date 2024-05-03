#include "var.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/proc/profile.h"
#include "lightos/syscall.h"

HANDLE open_sysvar_ex(HANDLE handle, char* key, uint16_t flags)
{
  if (!key)
    return HNDL_INVAL;

  return syscall_3(SYSID_OPEN_SYSVAR, (uint64_t)key, handle, flags);
}

HANDLE open_sysvar(char* key, uint16_t flags)
{
  if (!key)
    return HNDL_INVAL;

  return open_handle(key, HNDL_TYPE_SYSVAR, flags, 0);
}

BOOL create_sysvar(HANDLE handle, const char* key, enum SYSVAR_TYPE type, DWORD flags, VOID* value)
{
  if (!key)
    return FALSE;

  /* Make sure this is a bool and not something like a qword */
  return (syscall_5(SYSID_CREATE_SYSVAR, handle, (uintptr_t)key, type, flags, (uintptr_t)value) == TRUE);
}

BOOL sysvar_get_type(HANDLE var_handle, enum SYSVAR_TYPE* type)
{
  if (!type)
    return FALSE;

  return syscall_2(SYSID_GET_SYSVAR_TYPE, var_handle, (uint64_t)type);
}

BOOL sysvar_read(HANDLE handle, QWORD buffer_size, void* buffer)
{
  HANDLE_TYPE type;

  if (!get_handle_type(handle, &type) || type != HNDL_TYPE_SYSVAR)
    return FALSE;

  return handle_read(handle, buffer_size, buffer);
}

BOOL sysvar_write(HANDLE handle, QWORD buffer_size, void* buffer)
{
  HANDLE_TYPE type;

  if (!get_handle_type(handle, &type) || type != HNDL_TYPE_SYSVAR)
    return FALSE;

  return handle_write(handle, buffer_size, buffer);
}

/*
 * NOTE: we open the profile and the varialbe with the same flags
 */
BOOL sysvar_read_from_profile(char* profile_name, char* var_key, WORD flags, QWORD buffer_size, void* buffer)
{
  HANDLE profile_handle;
  HANDLE var_handle;

  if (!buffer || !buffer_size)
    return FALSE;

  profile_handle = open_profile(profile_name, flags | HNDL_FLAG_READACCESS);

  /* Yikes */
  if (!handle_verify(profile_handle))
    return FALSE;

  var_handle = open_sysvar_ex(profile_handle, var_key, flags | HNDL_FLAG_READACCESS);

  /* Failed to open a handle to the variable, close the profile and exit */
  if (!handle_verify(var_handle)) {
    close_handle(profile_handle);
    return FALSE;
  }

  /* Try to read the variable and simply exit with the result value */
  sysvar_read(var_handle, buffer_size, buffer);

  /* Close variable */
  close_handle(var_handle);

  /* Close profile */
  close_handle(profile_handle);

  return TRUE;
}

/*
 * NOTE: we open the profile and the varialbe with the same flags
 */
BOOL sysvar_write_from_profile(char* profile_name, char* var_key, WORD flags, QWORD buffer_size, void* buffer)
{
  BOOL result;
  HANDLE profile_handle;
  HANDLE var_handle;

  profile_handle = open_profile(profile_name, flags | HNDL_FLAG_WRITEACCESS);

  /* Yikes */
  if (!handle_verify(profile_handle))
    return FALSE;

  var_handle = open_sysvar_ex(profile_handle, var_key, flags | HNDL_FLAG_WRITEACCESS);

  /* Failed to open a handle to the variable, close the profile and exit */
  if (!handle_verify(var_handle)) {
    close_handle(profile_handle);
    return FALSE;
  }

  /* Try to read the variable and simply exit with the result value */
  result = sysvar_write(var_handle, buffer_size, buffer);

  /* Close variable */
  close_handle(var_handle);

  /* Close profile */
  close_handle(profile_handle);

  return result;
}
