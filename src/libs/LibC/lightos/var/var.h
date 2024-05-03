#ifndef __LIGHTOS_VAR_SYSVAR__
#define __LIGHTOS_VAR_SYSVAR__

#include "lightos/handle_def.h"
#include "lightos/proc/var_types.h"
#include "lightos/system.h"

/*!
 * @brief: Open a sysvar from a handle
 *
 * @handle: A handle to either a user profile or penv. Both of these can contain sysvars
 * @key: The sysvar key. This will always be capital
 * @flags: The flags with which to open the sysvar
 */
HANDLE open_sysvar(
 __IN__ HANDLE handle,
 __IN__ char* key,
 __IN__ WORD flags
);

/*
 * Try to create a variable
 *
 * Works on both profiles and process environments
 *
 * Fails if the variable already exists
 * Fails if we don't have permission to create variables on this profile
 */
BOOL create_sysvar(
 __IN__ HANDLE handle,
 __IN__ const char* key,
 __IN__ enum SYSVAR_TYPE type, 
 __IN__ DWORD flags,
 __IN__ VOID* value
);

/*
 * Grab the type of a certain profile variable
 */
BOOL sysvar_get_type(
 __IN__ HANDLE var_handle,
 __OUT__ enum SYSVAR_TYPE* type
);

/*
 * Read from a system variable
 */
BOOL sysvar_read(
 __IN__ HANDLE var_handle,
 __IN__ QWORD buffer_size,
 __OUT__ void* buffer
);

/*
 * Write to a system variable
 */
BOOL sysvar_write(
 __IN__ HANDLE var_handle,
 __IN__ QWORD buffer_size,
 __IN__ void* buffer
);


/*!
 * @brief: Read from a system variable on a profile
 *
 */
BOOL sysvar_read_from_profile(
 __IN__ char* profile_name,
 __IN__ char* var_key,
 __IN__ WORD flags,
 __IN__ QWORD buffer_size,
 __OUT__ void* buffer
);

/*
 * Write to a profile variable in one go
 */
BOOL sysvar_write_from_profile(
 __IN__ char* profile_name,
 __IN__ char* var_key,
 __IN__ WORD flags,
 __IN__ QWORD buffer_size,
 __OUT__ void* buffer
);


#endif // !__LIGHTOS_VAR_SYSVAR__
