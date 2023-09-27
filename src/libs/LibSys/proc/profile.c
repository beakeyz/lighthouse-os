#include "profile.h"
#include "LibSys/handle.h"
#include "LibSys/handle_def.h"

HANDLE open_profile(const char* name, DWORD flags)
{
  return open_handle(name, HNDL_TYPE_PROFILE, flags, HNDL_MODE_SCAN_PROFILE);
}

BOOL profile_read_var(HANDLE handle, const char* key, QWORD buffer_size, void* buffer)
{
  return FALSE;
}
