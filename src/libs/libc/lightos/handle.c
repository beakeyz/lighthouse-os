#include "handle.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "lightos/system.h"
#include "stdlib.h"
#include <stdio.h>

/*
 * Use the value of the handle to detirmine if it might be
 * invalide, otherwise use a syscall (TODO) to verify if the
 * handle gives a valid result
 */
BOOL handle_verify(handle_t handle)
{
    if (handle < 0)
        return FALSE;

    /* TODO: syscall for verification */

    return TRUE;
}

BOOL get_handle_type(HANDLE handle, HANDLE_TYPE* type)
{
    char t = syscall_1(SYSID_GET_HNDL_TYPE, handle);

    if (t < 0)
        return FALSE;

    /* NOTE: HANDLE_TYPE is unsigned */
    *type = t;

    return TRUE;
}

BOOL handle_expand_type(HANDLE handle)
{
    exit_noimpl("TODO: handle_expand_type");
    return FALSE;
}

BOOL handle_reclassify(HANDLE handle, HANDLE_TYPE type)
{
    exit_noimpl("TODO: handle_reclassify");
    return FALSE;
}

BOOL close_handle(HANDLE handle)
{
    QWORD result = syscall_1(SYSID_CLOSE, handle);

    if (result == SYS_OK)
        return TRUE;

    return FALSE;
}

/*
 * Send a syscall to open a handle
 */
HANDLE
open_handle(const char* path, HANDLE_TYPE type, DWORD flags, DWORD mode)
{
    if (!path)
        path = "\0";

    return syscall_4(SYSID_OPEN, (uint64_t)path, type, flags, mode);
}

HANDLE
open_handle_rel(HANDLE rel_handle, const char* path, HANDLE_TYPE type, DWORD flags, DWORD mode)
{
    if (!path)
        return HNDL_INVAL;

    return syscall_5(SYSID_OPEN_REL, rel_handle, (uintptr_t)path, type, flags, mode);
}

BOOL handle_set_offset(HANDLE handle, QWORD offset)
{
    QWORD result;

    if (!handle_verify(handle))
        return FALSE;

    result = syscall_3(SYSID_SEEK, handle, offset, SEEK_SET);

    if (result != SYS_OK)
        return FALSE;

    return TRUE;
}

BOOL handle_get_offset(HANDLE handle, QWORD* boffset)
{
    QWORD value;

    if (!boffset || !handle_verify(handle))
        return FALSE;

    value = syscall_3(SYSID_SEEK, handle, NULL, SEEK_CUR);

    if (value == SYS_INV)
        return FALSE;

    *boffset = value;
    return TRUE;
}

BOOL handle_read(HANDLE handle, QWORD buffer_size, VOID* buffer)
{
    return (handle_read_ex(handle, buffer_size, buffer, NULL) == buffer_size);
}

BOOL handle_read_ex(HANDLE handle, QWORD buffer_size, VOID* buffer, QWORD* bBytesRead)
{
    QWORD bytes_read;

    if (!buffer_size || !buffer)
        return FALSE;

    bytes_read = syscall_3(SYSID_READ, handle, (uintptr_t)buffer, buffer_size);

    if (bBytesRead)
        *bBytesRead = bytes_read;

    return (bytes_read == buffer_size);
}

BOOL handle_write(HANDLE handle, QWORD buffer_size, VOID* buffer)
{
    uintptr_t error;

    /* NOTE: sys write returns a syscall statuscode */
    error = syscall_3(SYSID_WRITE, handle, (uintptr_t)buffer, buffer_size);

    if (error != SYS_OK)
        return FALSE;

    return TRUE;
}
