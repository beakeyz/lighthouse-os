#include "profile.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"

HANDLE open_profile(const char* name, DWORD flags)
{
    return open_handle(name, HNDL_TYPE_PROFILE, flags, HNDL_MODE_SCAN_PROFILE);
}

BOOL profile_open_from_process(HANDLE process_handle, HANDLE* profile_handle)
{
    return FALSE;
}
