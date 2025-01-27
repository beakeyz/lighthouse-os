#include "dir.h"
#include "libk/flow/error.h"
#include "lightos/api/objects.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "sync/mutex.h"
#include <oss/object.h>

static dir_ops_t _generic_dir_ops = {
    .f_destroy = NULL,
    .f_open_idx = NULL,
};

static int __destroy_dis(oss_object_t* object)
{
    dir_t* dir;

    if (!object || !object->private || object->type != OT_DIR)
        return -EINVAL;

    dir = object->private;

    if (dir->ops && dir->ops->f_destroy)
        dir->ops->f_destroy(dir);

    destroy_mutex(dir->lock);

    kfree(dir);

    return 0;
}

static int __oss_dir_open(oss_object_t* object, const char* path, oss_object_t** pobj)
{
    dir_t* dir;
    oss_object_t* ret;

    /* Check for valid params */
    if (!path || !pobj)
        return -EINVAL;

    /* Try to unwrap the object */
    dir = oss_object_unwrap(object, OT_DIR);

    if (!dir)
        return -EINVAL;

    if (!dir->ops->f_open)
        return -ENOIMPL;

    /* Call the directories find function */
    ret = dir->ops->f_open(dir, path);

    if (!ret)
        return -ENOENT;

    /* Export the thing */
    *pobj = ret;

    return 0;
}

static int __oss_dir_open_idx(oss_object_t* object, u32 idx, oss_object_t** pobj)
{
    dir_t* dir;
    oss_object_t* ret;

    /* Check for valid params */
    if (!pobj)
        return -EINVAL;

    /* Try to unwrap the object */
    dir = oss_object_unwrap(object, OT_DIR);

    if (!dir)
        return -EINVAL;

    if (!dir->ops->f_open_idx)
        return -ENOIMPL;

    /* Call the directories find function */
    ret = dir->ops->f_open_idx(dir, idx);

    if (!ret)
        return -ENOENT;

    /* Export the thing */
    *pobj = ret;

    return 0;
}

/*!
 * @brief: Call the underlying connect call for this object
 *
 * This call is meant to notify backend filesystems of a new object being connected
 *
 */
static int __oss_dir_connect(oss_object_t* object, oss_object_t* child)
{
    dir_t* dir;

    dir = oss_object_unwrap(object, OT_DIR);

    if (!dir)
        return -EINVAL;

    /* This is okay. A directory doen't need to implement this function */
    if (!dir->ops->f_connect_child)
        return ENOIMPL;

    return dir->ops->f_connect_child(dir, child);
}

/*!
 * @brief: Call to the underlying disconnect function of the directory
 *
 * Notify the backing filesystem of a disconnect event.
 * NOTE: When this is called, we may assume @child is actually connected
 *       to @object. If this isn't the case, we're fucked lol
 */
static int __oss_dir_disconnect(oss_object_t* object, oss_object_t* child)
{
    dir_t* dir;

    dir = oss_object_unwrap(object, OT_DIR);

    if (!dir)
        return -EINVAL;

    /* Check if we have this implemented */
    if (!dir->ops->f_disconnect_child)
        return ENOIMPL;

    /* Call it if we do */
    return dir->ops->f_disconnect_child(dir, child->key);
}

static oss_object_ops_t dir_oss_ops = {
    .f_Destroy = __destroy_dis,
    .f_Open = __oss_dir_open,
    .f_OpenIdx = __oss_dir_open_idx,
    .f_Connect = __oss_dir_connect,
    .f_Disconnect = __oss_dir_disconnect,
    .f_ConnectNew = NULL,
};

/*!
 * @brief: Create a directory at the end of @path
 *
 * Fails if there is already a directory attached to the target node
 *
 * NOTE: If there is a root specified, but not a path, we assume dir should be created onto @root
 *
 * @root: The root node of the filesystem this directory comes from
 * @path: The path relative from @root
 */
dir_t* create_dir(const char* key, struct dir_ops* ops, fs_root_object_t* fsroot, void* priv, uint32_t flags)
{
    dir_t* dir;

    /* This would be bad lmao */
    if (!key)
        return nullptr;

    dir = kmalloc(sizeof(*dir));

    if (!dir)
        return nullptr;

    if (!ops)
        ops = &_generic_dir_ops;

    dir->object = create_oss_object(key, NULL, OT_DIR, &dir_oss_ops, dir);
    dir->key = dir->object->key;
    dir->fsroot = fsroot;
    dir->ops = ops;
    dir->priv = priv;
    dir->flags = flags;
    dir->child_capacity = 0xFFFFFFFF;
    dir->size = 0xFFFFFFFF;
    dir->lock = create_mutex(NULL);

    return dir;
}

/*!
 * @brief: Ensure a directory gets attached to a node
 */
int dir_do_attach(dir_t* dir, oss_object_t* parent)
{
    if (!dir || !parent)
        return -EINVAL;

    /*
     * Connect the dir object to the parent. This increases the reference count, since
     * objects always keep a reference to their downstream connection objects
     */
    return oss_object_connect(parent, dir->object);
}

int dir_create_child(dir_t* dir, oss_object_t* child)
{
    if (!dir)
        return -EINVAL;

    /* Connect will try to call the backing oss function, before actually making the connection */
    return oss_object_connect(dir->object, child);
}

int dir_remove_child(dir_t* dir, oss_object_t* child)
{
    if (!dir)
        return -EINVAL;

    /* Oss object disconnect will handle everything for us */
    return oss_object_disconnect(dir->object, child);
}

/*!
 * @brief: Find an object relative to this directory object
 *
 * Simply uses the oss open system under the hood. Filesystem drivers
 * may implement their own dir open functions, in order to generate
 * oss objects from the backing system
 */
struct oss_object* dir_find(dir_t* dir, const char* path)
{
    oss_object_t* result = nullptr;

    if (!dir || !path)
        return nullptr;

    if (oss_open_object_from(path, dir->object, &result))
        return nullptr;

    return result;
}

struct oss_object* dir_find_idx(dir_t* dir, uint64_t idx)
{
    kernel_panic("TODO: implement dir_find_idx");
}

dir_t* dir_open_from(struct oss_object* rel, const char* path)
{
    oss_object_t* obj = nullptr;

    if (oss_open_object_from(path, rel, &obj) || !obj)
        return nullptr;

    /* Is this a correct object? */
    if (obj->private && obj->type == OT_DIR)
        return obj->private;

    /* Invalid object =( need to close it again */
    oss_object_close(obj);

    return nullptr;
}

dir_t* dir_open(const char* path)
{
    return dir_open_from(NULL, path);
}

/*!
 * @brief: Close a directory
 *
 * All this pretty much needs to do is check if we can kill the directory and it's owned memory
 * and kill it if we can. Ownership of the directory lays with the associated oss_node, so we'll have
 * to look at that to see if we can murder. If it has no attached objects, we can proceed to kill it.
 * otherwise we'll just have to detach it's child and revert it's type back to a storage node
 */
kerror_t dir_close(dir_t* dir)
{
    return oss_object_close(dir->object);
}
