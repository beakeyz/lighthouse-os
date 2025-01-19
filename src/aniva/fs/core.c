#include "core.h"
#include "fs/dir.h"
#include "fs/file.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/api/objects.h"
#include "mem/heap.h"
#include "oss/core.h"
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

oss_object_t* fsroot_mount(oss_object_t* object, fs_type_t* fstype, const char* mountpoint, volume_t* volume)
{
    error_t error;
    oss_object_t* fsroot_obj;

    if (!fstype || !fstype->f_mount)
        return nullptr;

    /* Call the mount routine of the filesystem driver */
    fsroot_obj = fstype->f_mount(fstype, mountpoint, volume);

    if (!fsroot_obj)
        return nullptr;

    if (!object)
        error = oss_connect_root_object(fsroot_obj);
    else
        /* Connect the new object to it's parent */
        error = oss_object_connect(object, fsroot_obj);

    if (error) {
        oss_object_close(fsroot_obj);
        return nullptr;
    }

    return fsroot_obj;
}

error_t fsroot_unmount(fs_root_object_t* fsroot)
{
    error_t error;
    oss_object_t* object;

    if (!fsroot || !fsroot->rootdir)
        return -EINVAL;

    object = fsroot->rootdir->object;

    if (fsroot->m_type->f_unmount) {
        /* Call the unmount function on the fsroot */
        error = fsroot->m_type->f_unmount(fsroot->m_type, object);

        if (error)
            return error;
    }

    /*
     * Clear the remaining connections, which should decrease the
     * object reference count back to zero, destroying the object
     */
    oss_object_close_upstream_connections(object);

    return 0;
}

static int __fsroot_object_purge(oss_object_t* object)
{
    oss_connection_t* conn;

    /* Loop over all this objects connections */
    FOREACH(i, object->connections)
    {
        conn = i->data;

        if (conn->parent == object) {
            __fsroot_object_purge(conn->parent);
        }
    }

    oss_object_close(object);

    return 0;
}

error_t fsroot_purge(fs_root_object_t* fsroot)
{
    oss_object_t* object;

    /* Grab the fsroots root object */
    object = fsroot->rootdir->object;

    /* Purge all this shit */
    return __fsroot_object_purge(object);
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
