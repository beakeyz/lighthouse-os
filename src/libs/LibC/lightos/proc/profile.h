#ifndef __LIGHTENV_LIBSYS_PROFILE__
#define __LIGHTENV_LIBSYS_PROFILE__

/*
 * Routines to deal with profiles and profile variables 
 */

#include "lightos/handle_def.h"
#include "lightos/system.h"
#include "sys/types.h"

/*
 * Open a profile from a process handle
 */
BOOL profile_open_from_process(
  __IN__ HANDLE process_handle,
  __OUT__ HANDLE* profile_handle
);

/*
 * Open a profile by its name
 */
HANDLE open_profile(
  __IN__ const char* name,
  __IN__ DWORD flags
);

/*
 * Grab the name of a given profile from its handle
 */
BOOL profile_get_name(
 __IN__ HANDLE profile_handle,
 __IN__ QWORD buffer_size,
 __OUT__ char* buffer
);

#endif // !__LIGHTENV_LIBSYS_PROFILE__
