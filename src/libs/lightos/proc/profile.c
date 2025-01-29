#include "profile.h"
#include "lightos/api/handle.h"
#include "lightos/handle.h"

HANDLE open_profile(const char* name, u32 flags)
{
    return open_handle(name, flags, OT_PROFILE, HNDL_MODE_NORMAL);
}

BOOL profile_open_from_process(HANDLE process_handle, HANDLE* profile_handle)
{
    return FALSE;
}
