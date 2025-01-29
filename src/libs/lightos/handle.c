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

error_t close_handle(HANDLE handle)
{
    return sys_close(handle);
}

HANDLE open_handle(const char* path, u32 flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode)
{
    if (!path)
        return HNDL_INVAL;

    return sys_open(path, handle_flags(flags, HNDL_INVAL), type, mode);
}

HANDLE open_handle_from(HANDLE rel_handle, const char* path, u32 flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode)
{
    if (!path)
        return HNDL_INVAL;

    return sys_open(path, handle_flags(flags, rel_handle), type, mode);
}

ssize_t handle_read(HANDLE handle, u64 offset, VOID* buffer, u64 buffer_size)
{
    return sys_read(handle, offset, buffer, buffer_size);
}

ssize_t handle_write(HANDLE handle, u64 offset, VOID* buffer, size_t buffer_size)
{
    if (!buffer || !buffer_size)
        return -EINVAL;

    return sys_write(handle, offset, buffer, buffer_size);
}
