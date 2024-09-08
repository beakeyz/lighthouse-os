#include "dir.h"
#include "dev/core.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "lightos/fs/shared.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"

static int _generic_dir_read(dir_t* dir, uint64_t idx, direntry_t* bentry)
{
    int error;
    dir_t* new_dir;
    oss_node_t* node;
    enum LIGHTOS_DIRENT_TYPE type;
    /* NOTE: We don't own this pointer */
    oss_node_entry_t* entry;

    node = dir->node;

    if (!node)
        return -KERR_NULL;

    error = oss_node_find_at(node, idx, &entry);

    if (error)
        return error;

    switch (entry->type) {
    case OSS_ENTRY_OBJECT:
        oss_obj_ref(entry->obj);

        switch (entry->obj->type) {
        case OSS_OBJ_TYPE_VAR:
            type = LIGHTOS_DIRENT_TYPE_VAR;
            break;
        case OSS_OBJ_TYPE_FILE:
            type = LIGHTOS_DIRENT_TYPE_FILE;
            break;
        case OSS_OBJ_TYPE_PROC:
            type = LIGHTOS_DIRENT_TYPE_PROC;
            break;
        case OSS_OBJ_TYPE_DEVICE:
            type = LIGHTOS_DIRENT_TYPE_DEV;
            break;
        case OSS_OBJ_TYPE_DRIVER:
            type = LIGHTOS_DIRENT_TYPE_DRV;
            break;
        default:
            type = LIGHTOS_DIRENT_TYPE_UNKNOWN;

            KLOG_ERR("Could not read dirent with object of type %lld\n", entry->obj->type);
            break;
        }

        init_direntry(bentry, entry->obj, type);
        break;
    case OSS_ENTRY_NESTED_NODE:

        new_dir = entry->node->dir;

        if (!new_dir) {
            /* Don't give a path, as to signal we want to create this dir onto the specified node */
            new_dir = create_dir(entry->node->parent, entry->node->name, NULL, NULL, NULL);

            /* Attach the dir, since that's not yet done */
            oss_node_attach_dir(entry->node, new_dir);
        }

        /* Reference the new directory */
        dir_ref(new_dir);

        /* Init that bitch */
        init_direntry(bentry, new_dir, LIGHTOS_DIRENT_TYPE_DIR);
        break;
    }

    return 0;
}

static dir_ops_t _generic_dir_ops = {
    .f_destroy = NULL,
    .f_read = _generic_dir_read,
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
dir_t* create_dir(oss_node_t* root, const char* path, struct dir_ops* ops, void* priv, uint32_t flags)
{
    dir_t* dir;
    const char* name = NULL;

    /* This would be bad lmao */
    if (!root || !path)
        return nullptr;

    /* NOTE: This allocates @name on the heap */
    name = oss_get_objname(path);

    if (!name)
        return nullptr;

    dir = kmalloc(sizeof(*dir));

    if (!dir) {
        kfree((void*)name);
        return nullptr;
    }

    memset(dir, 0, sizeof(*dir));

    if (!ops)
        ops = &_generic_dir_ops;

    dir->name = name;
    dir->rootnode = root;
    dir->ops = ops;
    dir->priv = priv;
    dir->flags = flags;
    dir->child_capacity = 0xFFFFFFFF;
    dir->size = 0xFFFFFFFF;
    dir->lock = create_mutex(NULL);

    init_atomic_ptr(&dir->ref, 0);

    return dir;
}

dir_t* create_dir_on_node(struct oss_node* node, struct dir_ops* ops, void* priv, uint32_t flags)
{
    dir_t* ret;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    if (!ops)
        ops = &_generic_dir_ops;

    ret->name = strdup(node->name);
    ret->node = node;
    ret->ops = ops;
    ret->priv = priv;
    ret->flags = flags;
    ret->child_capacity = 0xFFFFFFFF;
    ret->size = 0xFFFFFFFF;
    ret->lock = create_mutex(NULL);

    init_atomic_ptr(&ret->ref, 0);

    oss_node_attach_dir(node, ret);

    return ret;
}

void dir_ref(dir_t* dir)
{
    if (!dir)
        return;

    mutex_lock(dir->lock);

    /* Increase the refcount by one */
    atomic_ptr_write(&dir->ref,
        atomic_ptr_read(&dir->ref) + 1);

    mutex_unlock(dir->lock);
}

void dir_unref(dir_t* dir)
{
    uint32_t refc;

    if (!dir)
        return;

    mutex_lock(dir->lock);

    refc = atomic_ptr_read(&dir->ref);

    if (!refc)
        goto unlock_and_exit;

    /* Increase the refcount by one */
    atomic_ptr_write(&dir->ref,
        (refc = refc - 1));

unlock_and_exit:
    mutex_unlock(dir->lock);

    if (!refc)
        destroy_dir(dir);
}

void destroy_dir(dir_t* dir)
{
    if (!dir)
        return;

    if (dir->ops && dir->ops->f_destroy)
        dir->ops->f_destroy(dir);

    /* Detach first */
    if (dir->node)
        oss_node_detach_dir(dir->node);

    destroy_mutex(dir->lock);

    kfree((void*)dir->name);
    kfree(dir);
}

/*!
 * @brief: Ensure a directory gets attached to a node
 */
int dir_do_attach(dir_t* dir, const char* path)
{
    if (dir->node) {
        KLOG_DBG("Already had a node: %s\n", dir->node->name);
        return 0;
    }

    KLOG_DBG("Creating path: (%s)/%s\n", dir->rootnode->name, path);

    dir->node = oss_create_path(dir->rootnode, path);

    /* Huh */
    if (!dir->node)
        return -KERR_INVAL;

    /* Attach the dir to it's node */
    oss_node_attach_dir(dir->node, dir);
    return 0;
}

int dir_create_child(dir_t* dir, const char* name)
{
    if (!dir || !name)
        return -KERR_INVAL;

    if (!dir->ops || !dir->ops->f_create_child)
        return -KERR_NULL;

    return dir->ops->f_create_child(dir, name);
}

int dir_remove_child(dir_t* dir, const char* name)
{
    if (!dir || !name)
        return -KERR_INVAL;

    if (!dir->ops || !dir->ops->f_remove_child)
        return -KERR_NULL;

    return dir->ops->f_remove_child(dir, name);
}

struct oss_obj* dir_find(dir_t* dir, const char* path)
{
    if (!dir || !path)
        return nullptr;

    if (!dir->ops || !dir->ops->f_find)
        return nullptr;

    return dir->ops->f_find(dir, path);
}

int dir_read(dir_t* dir, uint64_t idx, direntry_t* bentry)
{
    if (!dir)
        return -KERR_INVAL;

    if (!dir->ops || !dir->ops->f_read)
        return -KERR_INVAL;

    return dir->ops->f_read(dir, idx, bentry);
}

dir_t* dir_open_from(oss_node_t* rel, const char* path)
{
    dir_t* ret;
    oss_node_t* orig_node;
    oss_node_t* node = NULL;

    oss_resolve_node_rel(rel, path, &node);

    if (!node)
        return nullptr;

    ret = node->dir;

    /* This node does not have a directory, so we'll probably have to query the gen node */
    if (ret)
        goto ref_and_exit;

    orig_node = node;

    do {
        node = node->parent;
    } while (node && node->type != OSS_OBJ_GEN_NODE);

    /* No object generation node somewhere downstream, just create a new generic dir */
    if (!node)
        return create_dir_on_node(orig_node, NULL, NULL, NULL);

    uintptr_t idx = 0;
    const char* rel_path = nullptr;

    while (path[idx]) {
        /* We've found the node name in our path yay */
        if (strncmp(&path[idx], node->name, strlen(node->name)) == 0) {
            rel_path = &path[idx + strlen(node->name) + 1];
            break;
        }

        idx++;
    }

    /* Scan failed */
    if (!rel_path)
        return nullptr;

    /* In this case there should be a filesystem driver which initializes the dir struct correctly for us */
    if (!KERR_OK(oss_node_query_node(node, rel_path, &node)))
        return nullptr;

    ret = node->dir;

ref_and_exit:
    dir_ref(ret);
    return ret;
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
    dir_unref(dir);
    return 0;
}

kerror_t init_direntry(direntry_t* dirent, void* entry, enum LIGHTOS_DIRENT_TYPE type)
{
    if (!dirent || !entry)
        return -KERR_INVAL;

    dirent->_entry = entry;
    dirent->type = type;

    return 0;
}

kerror_t close_direntry(direntry_t* entry)
{
    if (!entry || !entry->_entry)
        return -KERR_INVAL;

    switch (entry->type) {
    case LIGHTOS_DIRENT_TYPE_FILE:
        file_close(entry->file);
        break;
    case LIGHTOS_DIRENT_TYPE_DIR:
        dir_close(entry->dir);
        break;
    case LIGHTOS_DIRENT_TYPE_OBJ:
    case LIGHTOS_DIRENT_TYPE_DEV:
    case LIGHTOS_DIRENT_TYPE_DRV:
    case LIGHTOS_DIRENT_TYPE_VAR:
    case LIGHTOS_DIRENT_TYPE_PROC:
        oss_obj_close(entry->obj);
        break;
    default:
        kernel_panic("TODO: unhandled direntry type in close_direntry");
    }

    return 0;
}
