#include "profile.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "lightos/system.h"

HANDLE open_profile(const char* name, DWORD flags)
{
  return open_handle(name, HNDL_TYPE_PROFILE, flags, HNDL_MODE_SCAN_PROFILE);
}

HANDLE open_profile_variable(char* key, HANDLE profile_handle, uint16_t flags)
{
  if (!key)
    return HNDL_INVAL;

  return syscall_3(SYSID_OPEN_PVAR, (uint64_t)key, profile_handle, flags);
}

BOOL profile_open_from_process(HANDLE process_handle, HANDLE* profile_handle)
{
  return FALSE;
}

BOOL create_profile_variable(HANDLE profile_handle, const char* key, enum PROFILE_VAR_TYPE type, DWORD flags, VOID* value)
{
  if (!key)
    return FALSE;

  /* Make sure this is a bool and not something like a qword */
  return (syscall_5(SYSID_CREATE_PVAR, profile_handle, (uintptr_t)key, type, flags, (uintptr_t)value) == TRUE);
}

BOOL profile_var_get_type(HANDLE var_handle, enum PROFILE_VAR_TYPE* type)
{
  if (!type)
    return FALSE;

  return syscall_2(SYSID_GET_PVAR_TYPE, var_handle, (uint64_t)type);
}

BOOL profile_var_read(HANDLE handle, QWORD buffer_size, void* buffer)
{
  HANDLE_TYPE type;

  if (!get_handle_type(handle, &type) || type != HNDL_TYPE_PVAR)
    return FALSE;

  return handle_read(handle, buffer_size, buffer);
}

BOOL profile_var_write(HANDLE handle, QWORD buffer_size, void* buffer)
{
  HANDLE_TYPE type;

  if (!get_handle_type(handle, &type) || type != HNDL_TYPE_PVAR)
    return FALSE;

  return handle_write(handle, buffer_size, buffer);
}

/*
 * NOTE: we open the profile and the varialbe with the same flags
 */
BOOL profile_var_read_ex(char* profile_name, char* var_key, WORD flags, QWORD buffer_size, void* buffer)
{
  HANDLE profile_handle;
  HANDLE var_handle;

  if (!buffer || !buffer_size)
    return FALSE;

  profile_handle = open_profile(profile_name, flags | HNDL_FLAG_READACCESS);

  /* Yikes */
  if (!handle_verify(profile_handle))
    return FALSE;

  var_handle = open_profile_variable(var_key, profile_handle, flags | HNDL_FLAG_READACCESS);

  /* Failed to open a handle to the variable, close the profile and exit */
  if (!handle_verify(var_handle)) {
    close_handle(profile_handle);
    return FALSE;
  }

  /* Try to read the variable and simply exit with the result value */
  profile_var_read(var_handle, buffer_size, buffer);

  /* Close variable */
  close_handle(var_handle);

  /* Close profile */
  close_handle(profile_handle);

  return TRUE;
}

/*
 * NOTE: we open the profile and the varialbe with the same flags
 */
BOOL profile_var_write_ex(char* profile_name, char* var_key, WORD flags, QWORD buffer_size, void* buffer)
{
  BOOL result;
  HANDLE profile_handle;
  HANDLE var_handle;

  profile_handle = open_profile(profile_name, flags | HNDL_FLAG_WRITEACCESS);

  /* Yikes */
  if (!handle_verify(profile_handle))
    return FALSE;

  var_handle = open_profile_variable(var_key, profile_handle, flags | HNDL_FLAG_WRITEACCESS);

  /* Failed to open a handle to the variable, close the profile and exit */
  if (!handle_verify(var_handle)) {
    close_handle(profile_handle);
    return FALSE;
  }

  /* Try to read the variable and simply exit with the result value */
  result = profile_var_write(var_handle, buffer_size, buffer);

  /* Close variable */
  close_handle(var_handle);

  /* Close profile */
  close_handle(profile_handle);

  return result;
}

