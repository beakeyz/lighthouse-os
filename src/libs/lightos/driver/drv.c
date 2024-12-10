#include "drv.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"
#include "stdlib.h"
#include "sys/types.h"

BOOL open_driver(const char* name, u32 flags, u32 mode, HANDLE* handle)
{
    error_t error;
    HANDLE ret;
    HANDLE_TYPE type = HNDL_TYPE_DRIVER;

    if (!name || !name[0])
        return FALSE;

    ret = open_handle(name, type, flags, mode);

    if (ret == HNDL_INVAL)
        goto fail_exit;

    error = handle_get_type(ret, &type);

    if (error) {
        close_handle(ret);
        goto fail_exit;
    }

    if (handle)
        *handle = ret;

    return TRUE;

fail_exit:
    *handle = HNDL_INVAL;
    return FALSE;
}

BOOL driver_send_msg(HANDLE handle, u32 code, u64 offset, VOID* buffer, size_t size)
{
    error_t error;
    error_t sys_status;
    HANDLE_TYPE type = HNDL_TYPE_NONE;

    /* Can't give either a size but or a buffer */
    if ((!buffer && size) || (buffer && !size))
        return FALSE;

    error = handle_get_type(handle, &type);

    if (error || type != HNDL_TYPE_DRIVER)
        return FALSE;

    sys_status = sys_send_msg(handle, code, offset, buffer, size);

    if (sys_status != 0)
        return FALSE;

    return TRUE;
}

BOOL load_driver(const char* path, u32 flags, HANDLE* handle)
{
    return open_driver(path, flags, HNDL_MODE_CREATE, handle);
}

BOOL unload_driver(HANDLE handle)
{
    exit_noimpl("unload_driver");
    return TRUE;
}

BOOL driver_query_info(HANDLE handle, drv_info_t* info_buffer)
{
    return FALSE;
}
