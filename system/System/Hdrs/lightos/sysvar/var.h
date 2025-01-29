#ifndef __LIGHTOS_PVR_ULIB__
#define __LIGHTOS_PVR_ULIB__

#include "lightos/api/sysvar.h"
#include <lightos/handle.h>

/*!
 * @brief: Open a sysvar from a handle
 *
 * @handle: A handle to either a user profile or penv. Both of these can contain sysvars
 * @key: The sysvar key. This will always be capital
 * @flags: The flags with which to open the sysvar
 */
extern HANDLE open_sysvar_ex(HANDLE handle, char* key, u32 flags);

/*!
 * @brief: Open a sysvar
 *
 * @key: The sysvar key. This will always be capital
 * @flags: The flags with which to open the sysvar
 */
extern HANDLE open_sysvar(char* key, u32 flags);

/*
 * Try to create a variable
 *
 * Works on both profiles and process environments
 *
 * Fails if the variable already exists
 * Fails if we don't have permission to create variables on this profile
 */
extern HANDLE create_sysvar(HANDLE handle, const char* key, enum SYSVAR_TYPE type, u32 flags, VOID* value, size_t len);

/*
 * Grab the type of a certain profile variable
 */
extern BOOL sysvar_get_type(HANDLE var_handle, enum SYSVAR_TYPE* type);

/*
 * Read from a system variable
 */
extern ssize_t sysvar_read(HANDLE var_handle, void* buffer, size_t buffer_size);

extern ssize_t sysvar_read_byte(HANDLE h_var, u8* pvalue);
/*
 * Write to a system variable
 */
extern ssize_t sysvar_write(HANDLE var_handle, void* buffer, size_t bsize);

/*!
 * @brief: Read from a system variable on a profile
 *
 */
extern BOOL sysvar_read_from_profile(char* profile_name, char* var_key, u32 flags, void* buffer, size_t buffer_size);

/*
 * Write to a profile variable in one go
 */
extern BOOL sysvar_write_from_profile(char* profile_name, char* var_key, u32 flags, void* buffer, size_t buffer_size);

/*!
 * @brief: Loads a PVR file into the environment of the current process
 *
 *
 */
extern int load_pvr(const char* path);
extern int load_pvr_ex(const char* path, HANDLE env);

int __lightos_init_sysvars(HANDLE procHandle);

#endif // !__LIGHTOS_PVR_ULIB__
