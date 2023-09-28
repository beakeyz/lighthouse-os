#include "profile.h"
#include "LibSys/handle.h"
#include "LibSys/handle_def.h"
#include "LibSys/syscall.h"
#include "LibSys/system.h"

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
