#include "var.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/api/sysvar.h"
#include "lightos/handle.h"
#include "lightos/proc/profile.h"
#include "lightos/syscall.h"
#include <stdlib.h>

/* Handle to the current process object */
static HANDLE __proc_handle;

HANDLE open_sysvar_ex(HANDLE handle, char* key, u32 flags)
{
    return open_handle_from(handle, key, flags, OT_SYSVAR, NULL);
}

HANDLE open_sysvar(char* key, u32 flags)
{
    if (!key)
        return HNDL_INVAL;

    return open_sysvar_ex(__proc_handle, key, flags);
}

extern HANDLE create_sysvar(HANDLE handle, const char* key, enum SYSVAR_TYPE type, u32 flags, VOID* value, size_t len)
{
    if (!key)
        return FALSE;

    /* Make sure this is a bool and not something like a u64 */
    return sys_create_sysvar(key, handle_flags(flags, handle), type, value, len);
}

BOOL sysvar_get_type(HANDLE var_handle, enum SYSVAR_TYPE* type)
{
    if (!type)
        return FALSE;

    *type = sys_get_sysvar_type(var_handle);
    return TRUE;
}

ssize_t sysvar_read(HANDLE var_handle, void* buffer, size_t buffer_size)
{
    return handle_read(var_handle, 0, buffer, buffer_size);
}

ssize_t sysvar_read_byte(HANDLE h_var, u8* pvalue)
{
    enum SYSVAR_TYPE type;

    /* Verify the handle */
    if (handle_verify(h_var))
        return 0;

    /* Get the type */
    if (!sysvar_get_type(h_var, &type))
        return 0;

    if (type != SYSVAR_TYPE_BYTE)
        return 0;

    return sysvar_read(h_var, pvalue, sizeof(*pvalue));
}

ssize_t sysvar_write(HANDLE var_handle, void* buffer, size_t bsize)
{
    return handle_write(var_handle, 0, buffer, bsize);
}

/*
 * NOTE: we open the profile and the varialbe with the same flags
 */
BOOL sysvar_read_from_profile(char* profile_name, char* var_key, u32 flags, void* buffer, size_t bsize)
{
    HANDLE profile_handle;
    HANDLE var_handle;

    if (!buffer || !bsize)
        return FALSE;

    profile_handle = open_profile(profile_name, flags | HF_READACCESS);

    /* Yikes */
    if (handle_verify(profile_handle))
        return FALSE;

    var_handle = open_sysvar_ex(profile_handle, var_key, flags | HF_READACCESS);

    /* Failed to open a handle to the variable, close the profile and exit */
    if (handle_verify(var_handle)) {
        close_handle(profile_handle);
        return FALSE;
    }

    /* Try to read the variable and simply exit with the result value */
    sysvar_read(var_handle, buffer, bsize);

    /* Close variable */
    close_handle(var_handle);

    /* Close profile */
    close_handle(profile_handle);

    return TRUE;
}

/*
 * NOTE: we open the profile and the varialbe with the same flags
 */
BOOL sysvar_write_from_profile(char* profile_name, char* var_key, u32 flags, void* buffer, size_t bsize)
{
    BOOL result;
    HANDLE profile_handle;
    HANDLE var_handle;

    profile_handle = open_profile(profile_name, flags | HF_WRITEACCESS);

    /* Yikes */
    if (handle_verify(profile_handle))
        return FALSE;

    var_handle = open_sysvar_ex(profile_handle, var_key, flags | HF_WRITEACCESS);

    /* Failed to open a handle to the variable, close the profile and exit */
    if (handle_verify(var_handle)) {
        close_handle(profile_handle);
        return FALSE;
    }

    /* Try to read the variable and simply exit with the result value */
    result = sysvar_write(var_handle, buffer, bsize);

    /* Close variable */
    close_handle(var_handle);

    /* Close profile */
    close_handle(profile_handle);

    return result;
}

int __lightos_init_sysvars(HANDLE procHandle)
{
    __proc_handle = procHandle;
    return 0;
}
