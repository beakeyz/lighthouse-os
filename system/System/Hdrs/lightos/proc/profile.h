#ifndef __LIGHTENV_LIBSYS_PROFILE__
#define __LIGHTENV_LIBSYS_PROFILE__

/*
 * Routines to deal with profiles and profile variables
 */

#include "lightos/api/handle.h"
#include "sys/types.h"

/*
 * Open a profile from a process handle
 */
BOOL profile_open_from_process(HANDLE process_handle, HANDLE* profile_handle);

/*
 * Open a profile by its name
 */
HANDLE open_profile(const char* name, u32 flags);

/*
 * Grab the name of a given profile from its handle
 */
BOOL profile_get_name(HANDLE profile_handle, char* buffer, size_t buffer_size);

#endif // !__LIGHTENV_LIBSYS_PROFILE__
