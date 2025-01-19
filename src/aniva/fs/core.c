#include "core.h"
#include "fs/dir.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "lightos/api/objects.h"
#include "mem/heap.h"
#include "oss/object.h"
#include <libk/stddef.h>
#include <libk/string.h>
#include <sync/mutex.h>

static fs_type_t* fsystems;
static mutex_t* fsystems_lock;

static fs_type_t** find_fs_type(const char* name, size_t length)
{
    fs_type_t** ret;

    for (ret = &fsystems; *ret; ret = &(*ret)->m_next) {
        /* Check if the names match and if ->m_name is a null-terminated string */
        if (memcmp((*ret)->m_name, name, length) && !(*ret)->m_name[length]) {
            break;
        }
    }

    return ret;
}

fs_type_t* get_fs_driver(fs_type_t* fs)
{
    kernel_panic("TODO: implement get_fs_driver");
}

kerror_t register_filesystem(fs_type_t* fs)
{
    fs_type_t** ptr;

    if (fs->m_next)
        return -1;

    mutex_lock(fsystems_lock);

    ptr = find_fs_type(fs->m_name, strlen(fs->m_name));

    if (*ptr) {
        return -1;
    } else {
        *ptr = fs;
    }

    mutex_unlock(fsystems_lock);

    return (0);
}

kerror_t unregister_filesystem(fs_type_t* fs)
{
    kernel_panic("TODO: test unregister_filesystem");

    fs_type_t** itterator;

    mutex_lock(fsystems_lock);

    itterator = &fsystems;

    while (*itterator) {
        if (*itterator == fs) {

            *itterator = fs->m_next;
            fs->m_next = nullptr;

            mutex_unlock(fsystems_lock);
            return (0);
        }

        itterator = &(*itterator)->m_next;
    }

    mutex_unlock(fsystems_lock);
    return -1;
}

static fs_type_t* __get_fs_type(const char* name, uint32_t length)
{
    fs_type_t* ret;

    mutex_lock(fsystems_lock);

    ret = *(find_fs_type(name, length));

    /* Don't dynamicaly load and the driver does not have to be active */
    if (ret && !try_driver_get(ret->m_driver, NULL)) {
        kernel_panic("Could not find driver");
        ret = NULL;
    }

    mutex_unlock(fsystems_lock);
    return ret;
}

fs_type_t* get_fs_type(const char* name)
{
    fs_type_t* ret;

    ret = __get_fs_type(name, strlen(name));

    if (!ret) {
        // Can we do anything to still try to get this filesystem?
    }

    return ret;
}

fs_root_object_t* create_fs_root_object(const char* name, fs_type_t* type, struct dir_ops* fsroot_ops, void* dir_priv)
{
    fs_root_object_t* fsnode;

    if (!name || !fsroot_ops)
        return nullptr;

    /*
     * Only allocate a fs node. It's up to the caller
     * to populate the fields
     */
    fsnode = kmalloc(sizeof(*fsnode));

    if (!fsnode)
        return nullptr;

    memset(fsnode, 0, sizeof(*fsnode));

    fsnode->m_type = type;
    fsnode->rootdir = create_dir(name, fsroot_ops, fsnode, dir_priv, DIR_FSROOT);

    return fsnode;
}

void destroy_fsroot_object(fs_root_object_t* object)
{
    dir_close(object->rootdir);

    kfree(object);
}

fs_root_object_t* oss_object_get_fsobj(oss_object_t* object)
{
    /* Get all the filesystem objects */
    file_t* file;
    dir_t* dir;

    dir = oss_object_unwrap(object, OT_DIR);

    if (dir)
        return dir->fsroot;

    file = oss_object_unwrap(object, OT_FILE);

    if (file)
        return file->fsroot;

    return nullptr;
}

/*!
 * @brief: Initialize the filesystem core
 *
 * Here we initialize the oss interface for regular fs-use
 */
void init_fs_core()
{
    fsystems_lock = create_mutex(0);
}
