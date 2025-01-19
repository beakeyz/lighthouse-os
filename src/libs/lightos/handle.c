#include "handle.h"
#include "lightos/api/handle.h"
#include "lightos/syscall.h"
#include "stdlib.h"
#include <stdio.h>

error_t handle_verify(HANDLE handle)
{
    if (handle < 0)
        return -EINVAL;
    return 0;
}

error_t handle_get_type(HANDLE handle, HANDLE_TYPE* type)
{
    if (!type)
        return -EINVAL;

    *type = sys_handle_get_type(handle);

    return 0;
}

error_t close_handle(HANDLE handle)
{
    return sys_close(handle);
}

HANDLE open_handle(const char* path, HANDLE_TYPE type, u32 flags, enum HNDL_MODE mode)
{
    if (!path)
        return HNDL_INVAL;

    return sys_open(path, handle_flags(flags, type, HNDL_INVAL), mode, NULL, NULL);
}

HANDLE open_handle_from(HANDLE rel_handle, const char* path, HANDLE_TYPE type, u32 flags, enum HNDL_MODE mode)
{
    if (!path)
        return HNDL_INVAL;

    return sys_open(path, handle_flags(flags, type, rel_handle), mode, NULL, NULL);
}

error_t handle_read(HANDLE handle, u64 offset, VOID* buffer, u64 buffer_size)
{
    return handle_read_ex(handle, offset, buffer, buffer_size, NULL);
}

error_t handle_read_ex(HANDLE handle, u64 offset, VOID* buffer, u64 buffer_size, u64* preadsize)
{
    if (!buffer || !buffer_size)
        return EINVAL;

    return sys_read(handle, offset, buffer, buffer_size, preadsize);
}

error_t handle_write(HANDLE handle, u64 offset, VOID* buffer, size_t buffer_size)
{
    if (!buffer || !buffer_size)
        return EINVAL;

    return sys_write(handle, offset, buffer, buffer_size);
}
