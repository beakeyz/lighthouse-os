#include "var.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/api/sysvar.h"
#include "lightos/handle.h"
#include "lightos/object.h"
#include "lightos/proc/profile.h"
#include "lightos/syscall.h"

HANDLE open_sysvar_ex(HANDLE handle, char* key, u32 flags)
{
    Object object;
    Object rel = { 0 };

    if (!key)
        return HNDL_INVAL;

    /* Set the dummy object */
    rel.handle = handle;

    object = OpenObjectFrom(&rel, key, flags, HNDL_MODE_NORMAL);

    if (object.type != OT_SYSVAR)
        CloseObject(&object);

    return object.handle;
}

HANDLE open_sysvar(char* key, u32 flags)
{
    Object object;

    if (!key)
        return HNDL_INVAL;

    object = OpenObject(key, flags, HNDL_MODE_NORMAL);

    if (object.type != OT_SYSVAR)
        CloseObject(&object);

    return object.handle;
}

extern HANDLE create_sysvar(HANDLE handle, const char* key, enum SYSVAR_TYPE type, u32 flags, VOID* value, size_t len)
{
    if (!key)
        return FALSE;

    /* Make sure this is a bool and not something like a u64 */
    return sys_create_sysvar(key, handle_flags(flags, HNDL_TYPE_OBJECT, handle), type, value, len);
}

BOOL sysvar_get_type(HANDLE var_handle, enum SYSVAR_TYPE* type)
{
    if (!type)
        return FALSE;

    *type = sys_get_sysvar_type(var_handle);
    return TRUE;
}

BOOL sysvar_read(HANDLE handle, void* buffer, u64 buffer_size)
{
    HANDLE_TYPE type;

    if (handle_get_type(handle, &type) || type != HNDL_TYPE_OBJECT)
        return FALSE;

    return handle_read(handle, 0, buffer, buffer_size);
}

extern BOOL sysvar_read_byte(HANDLE h_var, u8* pvalue)
{
    enum SYSVAR_TYPE type;

    /* Verify the handle */
    if (handle_verify(h_var))
        return FALSE;

    /* Get the type */
    if (!sysvar_get_type(h_var, &type))
        return FALSE;

    if (type != SYSVAR_TYPE_BYTE)
        return FALSE;

    return sysvar_read(h_var, pvalue, sizeof(*pvalue));
}

BOOL sysvar_write(HANDLE handle, void* buffer, size_t bsize)
{
    HANDLE_TYPE type;

    if (handle_get_type(handle, &type) || type != HNDL_TYPE_OBJECT)
        return FALSE;

    return handle_write(handle, 0, buffer, bsize);
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

    profile_handle = open_profile(profile_name, flags | HNDL_FLAG_READACCESS);

    /* Yikes */
    if (handle_verify(profile_handle))
        return FALSE;

    var_handle = open_sysvar_ex(profile_handle, var_key, flags | HNDL_FLAG_READACCESS);

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

    profile_handle = open_profile(profile_name, flags | HNDL_FLAG_WRITEACCESS);

    /* Yikes */
    if (handle_verify(profile_handle))
        return FALSE;

    var_handle = open_sysvar_ex(profile_handle, var_key, flags | HNDL_FLAG_WRITEACCESS);

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
