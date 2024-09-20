
#include "fs/file.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "oss/node.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"

static int file_khandle_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    file_t* file;
    handle_type_t type = driver->handle_type;

    if (!path)
        return -KERR_INVAL;

    file = file_open(path);

    if (!file)
        return -KERR_NOT_FOUND;

    init_khandle(bHandle, &type, file);

    return 0;
}

static int file_khandle_open_rel(khandle_driver_t* driver, khandle_t* rel, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    file_t* file;
    oss_node_t* rel_node;
    HANDLE_TYPE type = driver->handle_type;

    if (!rel)
        return -KERR_INVAL;

    rel_node = khandle_get_relative_node(rel);

    if (!rel_node)
        return -KERR_INVAL;

    /* Try to find an object on this node */
    file = file_open_from(rel_node, path);

    if (!file)
        return -KERR_NOT_FOUND;

    init_khandle(bHandle, &type, file);

    khandle_set_flags(bHandle, flags);

    return 0;
}

static int file_khdrv_close(khandle_driver_t* driver, khandle_t* handle)
{
    file_t* f;

    if (!handle)
        return -KERR_INVAL;

    f = handle->reference.file;

    if (!f)
        return -KERR_INVAL;

    file_close(f);
    return 0;
}

static int file_khandle_read(khandle_driver_t* driver, khandle_t* handle, void* buffer, size_t bsize)
{
    size_t read_len;
    file_t* file;

    file = handle->reference.file;

    if (!file)
        return -KERR_INVAL;

    read_len = file_read(file, buffer, bsize, handle->offset);

    if (!read_len)
        return -KERR_IO;

    /* Add to the offset field of the handle */
    handle->offset += read_len;
    return 0;
}

static int file_khandle_write(khandle_driver_t* driver, khandle_t* handle, void* buffer, size_t bsize)
{
    size_t write_len;
    file_t* file;

    file = handle->reference.file;

    if (!file)
        return -KERR_INVAL;

    if (file->m_flags & FILE_READONLY)
        return -KERR_NOPERM;

    write_len = file_write(file, buffer, bsize, handle->offset);

    if (!write_len)
        return -KERR_IO;

    /* Add to the offset field of the handle */
    handle->offset += write_len;
    return 0;
}

khandle_driver_t file_khandle_drv = {
    .name = "files",
    .handle_type = HNDL_TYPE_FILE,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL | KHDRIVER_FUNC_CLOSE | KHDRIVER_FUNC_READ | KHDRIVER_FUNC_WRITE,
    .f_open = file_khandle_open,
    .f_open_relative = file_khandle_open_rel,
    .f_close = file_khdrv_close,
    .f_read = file_khandle_read,
    .f_write = file_khandle_write,
};
EXPORT_KHANDLE_DRIVER(file_khandle_drv);
