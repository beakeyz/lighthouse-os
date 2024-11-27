#include "fs/dir.h"
#include "fs/file.h"
#include "lightos/fs/shared.h"
#include "lightos/handle_def.h"
#include "oss/node.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include <libk/string.h>

static int dir_khandle_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    dir_t* dir;
    HANDLE_TYPE type = driver->handle_type;

    if (!path)
        return HNDL_INVAL;

    dir = dir_open(path);

    if (!dir)
        return HNDL_NOT_FOUND;

    init_khandle_ex(bHandle, type, flags, dir);

    return 0;
}

static int dir_khandle_open_rel(khandle_driver_t* driver, khandle_t* rel, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    dir_t* dir;
    oss_node_t* rel_node;
    HANDLE_TYPE type = driver->handle_type;

    if (!rel)
        return -KERR_INVAL;

    rel_node = khandle_get_relative_node(rel);

    if (!rel_node)
        return -KERR_INVAL;

    /* Try to find an object on this node */
    dir = dir_open_from(rel_node, path);

    if (!dir)
        return -KERR_NOT_FOUND;

    init_khandle(bHandle, &type, dir);

    khandle_set_flags(bHandle, flags);
    return 0;
}

static int dir_khdrv_close(khandle_driver_t* driver, khandle_t* handle)
{
    dir_t* d;

    if (!handle)
        return -KERR_INVAL;

    d = handle->reference.dir;

    if (!d)
        return -KERR_INVAL;

    dir_close(d);
    return 0;
}

static int dir_khandle_read(khandle_driver_t* driver, khandle_t* handle, void* buffer, size_t bsize)
{
    u32 idx;
    dir_t* target_dir;
    const char* target_name;
    direntry_t target_entry = { 0 };
    lightos_direntry_t __user* user_direntry;

    user_direntry = buffer;
    idx = handle->offset;

    target_dir = handle->reference.dir;

    if (idx >= target_dir->child_capacity)
        return SYS_INV;

    /* Read into the entry */
    if (!KERR_OK(dir_read(target_dir, idx, &target_entry)))
        return SYS_INV;

    /* Clear the buffer */
    memset(user_direntry, 0, sizeof(*user_direntry));

    switch (target_entry.type) {
    case LIGHTOS_DIRENT_TYPE_DIR:
        target_name = target_entry.dir->name;
        break;
    case LIGHTOS_DIRENT_TYPE_FILE:
        target_name = target_entry.file->m_obj->name;
        break;
    default:
        target_name = target_entry.obj->name;
        break;
    }

    if (!target_name)
        goto close_and_fail;

    bsize = sizeof(user_direntry->name);

    /* Truncate the length */
    if (strlen(target_name) < bsize)
        bsize = strlen(target_name);

    /* Copy */
    strncpy(user_direntry->name, target_name, bsize);

    /* Set the direntry type */
    user_direntry->type = target_entry.type;

    close_direntry(&target_entry);
    return 0;

close_and_fail:
    close_direntry(&target_entry);
    return SYS_INV;
}

static khandle_driver_t dir_khandle_driver = {
    .name = "dir",
    .handle_type = HNDL_TYPE_DIR,
    .function_flags = KHDRIVER_FUNC_OPEN | KHDRIVER_FUNC_OPEN_REL | KHDRIVER_FUNC_CLOSE | KHDRIVER_FUNC_READ,
    .f_open = dir_khandle_open,
    .f_open_relative = dir_khandle_open_rel,
    .f_close = dir_khdrv_close,
    .f_read = dir_khandle_read,
};
EXPORT_KHANDLE_DRIVER(dir_khandle_driver);
