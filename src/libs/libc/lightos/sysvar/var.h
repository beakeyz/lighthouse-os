#ifndef __LIGHTOS_PVR_ULIB__
#define __LIGHTOS_PVR_ULIB__

#include "shared.h"
#include <lightos/handle.h>

/*!
 * @brief: Open a sysvar from a handle
 *
 * @handle: A handle to either a user profile or penv. Both of these can contain sysvars
 * @key: The sysvar key. This will always be capital
 * @flags: The flags with which to open the sysvar
 */
extern HANDLE open_sysvar_ex(
    __IN__ HANDLE handle,
    __IN__ char* key,
    __IN__ WORD flags);

/*!
 * @brief: Open a sysvar
 *
 * @key: The sysvar key. This will always be capital
 * @flags: The flags with which to open the sysvar
 */
extern HANDLE open_sysvar(
    __IN__ char* key,
    __IN__ WORD flags);

/*
 * Try to create a variable
 *
 * Works on both profiles and process environments
 *
 * Fails if the variable already exists
 * Fails if we don't have permission to create variables on this profile
 */
extern BOOL create_sysvar(
    __IN__ HANDLE handle,
    __IN__ const char* key,
    __IN__ enum SYSVAR_TYPE type,
    __IN__ DWORD flags,
    __IN__ VOID* value);

/*
 * Grab the type of a certain profile variable
 */
extern BOOL sysvar_get_type(
    __IN__ HANDLE var_handle,
    __OUT__ enum SYSVAR_TYPE* type);

/*
 * Read from a system variable
 */
extern BOOL sysvar_read(
    __IN__ HANDLE var_handle,
    __IN__ QWORD buffer_size,
    __OUT__ void* buffer);

/*
 * Write to a system variable
 */
extern BOOL sysvar_write(
    __IN__ HANDLE var_handle,
    __IN__ QWORD buffer_size,
    __IN__ void* buffer);

/*!
 * @brief: Read from a system variable on a profile
 *
 */
extern BOOL sysvar_read_from_profile(
    __IN__ char* profile_name,
    __IN__ char* var_key,
    __IN__ WORD flags,
    __IN__ QWORD buffer_size,
    __OUT__ void* buffer);

/*
 * Write to a profile variable in one go
 */
extern BOOL sysvar_write_from_profile(
    __IN__ char* profile_name,
    __IN__ char* var_key,
    __IN__ WORD flags,
    __IN__ QWORD buffer_size,
    __OUT__ void* buffer);

/*!
 * @brief: Loads a PVR file into the environment of the current process
 *
 *
 */
extern int load_pvr(const char* path);
extern int load_pvr_ex(const char* path, HANDLE env);

#endif // !__LIGHTOS_PVR_ULIB__
