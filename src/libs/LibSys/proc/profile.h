#ifndef __LIGHTENV_LIBSYS_PROFILE__
#define __LIGHTENV_LIBSYS_PROFILE__

/*
 * Routines to deal with profiles and profile variables 
 */

#include "LibSys/handle_def.h"
#include "LibSys/proc/var_types.h"
#include "LibSys/system.h"
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

/*
 * TODO: figure out how paths are going to works with 
 * profiles and their variables
 */
HANDLE open_profile_variable(
 __IN__ char* key,
 __IN__ HANDLE profile_handle,
 __IN__ uint16_t flags
);

/*
 * Grab the type of a certain profile variable
 */
BOOL profile_var_get_type(
 __IN__ HANDLE var_handle,
 __OUT__ enum PROFILE_VAR_TYPE* type
);

/*
 * Read from a profile variable
 */
BOOL profile_var_read(
 __IN__ HANDLE var_handle,
 __IN__ QWORD buffer_size,
 __OUT__ void* buffer
);

/*
 * Write to a profile variable
 */
BOOL profile_var_write(
 __IN__ HANDLE var_handle,
 __IN__ QWORD buffer_size,
 __IN__ void* buffer
);

#endif // !__LIGHTENV_LIBSYS_PROFILE__
