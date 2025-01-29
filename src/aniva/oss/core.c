#include "core.h"
#include "fs/core.h"
#include "fs/dir.h"
#include "libk/flow/error.h"
#include "lightos/api/objects.h"
#include "object.h"
#include "oss/connection.h"
#include "path.h"
#include <libk/data/hashmap.h>
#include <libk/string.h>
#include <sync/mutex.h>

#define MAX_OSS_ROOT_OBJS 1024

static hashmap_t* root_objects;
static mutex_t* oss_lock;
/*
 * Generic oss object by default don't implement any
 * oss methods
 */
static oss_object_ops_t __generic_oss_object_ops = {
    NULL,
};

static const char* __root_object_keys[] = {
    [ORT_STORAGE_MAIN] = "Storage",
    [ORT_DEVICES] = "Devices",
    [ORT_DRIVERS] = "Drivers",
    [ORT_TERMINALS] = "Terminals",
    [ORT_PROCESSES] = "Proc",
    [ORT_RUNTIME] = "Runtime",
};

const char* oss_get_default_rootobj_key(enum OSS_ROOTOBJ_TYPE type)
{
    if (type >= arrlen(__root_object_keys))
        return nullptr;

    return __root_object_keys[type];
}

error_t oss_open_root_object(const char* path, struct oss_object** pobj)
{
    oss_object_t* obj;

    if (!path || !pobj)
        return -EINVAL;

    mutex_lock(oss_lock);

    obj = hashmap_get(root_objects, (hashmap_key_t)path);

    mutex_unlock(oss_lock);

    if (!obj)
        return -ENOENT;

    *pobj = obj;
    return 0;
}

/*!
 * @brief: Might create a missing object
 */
static error_t __maybe_create_object(const char* key, u32 oss_path_idx, oss_path_t* oss_path, enum OSS_OBJECT_TYPE type, oss_object_t* rel, oss_object_t** pobj)
{
    /*
     * We can only try to create a new entry if we're at the last
     * subpath in an open action. Otherwise we don't know what to do
     */
    if (oss_path_idx != (oss_path->n_subpath - 1))
        return -ENOENT;

    /* Connect a new object and export a reference to that object */
    return oss_object_connect_new(rel, key, type, pobj);
}

/*!
 * @brief: Recursively open an object relative to a starting object
 *
 * NOTE: This function doesn't check parameters, so callers should do that
 * We simply walk all the connections relative to @rel. If there isn't a connection for a
 * certain object, we try to create one using the f_Open call on the object. If that fails,
 * we assume we've hit a dead end and we bail
 */
static error_t __oss_open_or_create_object_from(oss_path_t* path, u32 start_idx, enum OSS_OBJECT_TYPE create_type, oss_object_t* rel, oss_object_t** pobj)
{
    u32 i = start_idx;
    oss_object_t* walker = rel;
    oss_object_t* next_object = nullptr;

    /* Walk the object graph */
    while (walker && i < path->n_subpath) {
        if (oss_object_open(walker, path->subpath_vec[i], &next_object))
            /* Check if we want to try to create an object here. If we do, great! Otherwise return error */
            return (oss_object_valid_type(create_type) ? __maybe_create_object(path->subpath_vec[i], i, path, create_type, rel, pobj) : -ENOENT);

        /* Maybe connect this object, if that wasn't yet done */
        if (next_object)
            oss_connect(walker, next_object);

        i++;
        walker = next_object;
    }

    /* We need to still have a walker at the end of this */
    if (!walker || i != path->n_subpath)
        return -ENOENT;

    /* We've got an object! We do need to reference it lol */
    oss_object_ref(walker);

    /* Export the guy */
    *pobj = walker;

    return 0;
}

static error_t __oss_open_object(const char* path, enum OSS_OBJECT_TYPE create_type, struct oss_object** pobj)
{
    error_t error;
    oss_object_t* root_obj;
    oss_path_t oss_path;

    if (!path || !pobj)
        return -EINVAL;

    if (oss_parse_path(path, &oss_path))
        return -EINVAL;

    /* Try and grab a root node */
    error = oss_open_root_object(oss_path.subpath_vec[0], &root_obj);

    if (error)
        goto destroy_path_and_exit;

    /* Try to open the object relative to the root object we just found */
    error = __oss_open_or_create_object_from(&oss_path, 1, create_type, root_obj, pobj);

destroy_path_and_exit:
    oss_destroy_path(&oss_path);

    return error;
}

error_t oss_open_object(const char* path, struct oss_object** pobj)
{
    return __oss_open_object(path, OT_INVALID, pobj);
}

error_t oss_open_or_create_object(const char* path, enum OSS_OBJECT_TYPE type, struct oss_object** pobj)
{
    return __oss_open_object(path, type, pobj);
}

static error_t __oss_open_object_from(const char* path, enum OSS_OBJECT_TYPE create_type, struct oss_object* rel, struct oss_object** pobj)
{
    error_t error;
    oss_path_t oss_path;

    if (!rel || !pobj)
        return -EINVAL;

    /* Parse the provided path */
    if (oss_parse_path(path, &oss_path))
        return -EINVAL;

    /* Try to find the object */
    error = __oss_open_or_create_object_from(&oss_path, 0, create_type, rel, pobj);

    /* Kill the path */
    oss_destroy_path(&oss_path);

    return error;
}

error_t oss_open_object_from(const char* path, struct oss_object* rel, struct oss_object** pobj)
{
    return __oss_open_object_from(path, OT_INVALID, rel, pobj);
}

error_t oss_open_or_create_object_from(const char* path, enum OSS_OBJECT_TYPE type, struct oss_object* rel, struct oss_object** pobj)
{
    return __oss_open_object_from(path, type, rel, pobj);
}

/*!
 * @brief: Queries an object to open a new object based on index
 */
error_t oss_open_object_from_idx(u32 idx, struct oss_object* rel, struct oss_object** pobj)
{
    error_t error;
    oss_object_t* obj;
    oss_object_t* alr_connected_obj;

    if (!rel || !pobj)
        return -EINVAL;

    if (!rel->ops->f_OpenIdx)
        return -ENOIMPL;

    error = rel->ops->f_OpenIdx(rel, idx, &obj);

    if (error)
        return error;

    error = oss_connect(rel, obj);

    // KLOG_DBG("Connecting obj %s to rel %s\n", obj->key, rel->key);

    /* If this call fails, @obj is already connected to @rel. We'll grab and reference that guy */
    if (IS_OK(error))
        goto reference_and_return;

    /* Get the connected object from the relative boi */
    alr_connected_obj = oss_object_get_connected(rel, obj->key);

    /* Close the old object */
    oss_object_close(obj);

    /* Switch around the objects, since @alr_connected_obj still needs to be referenced */
    obj = alr_connected_obj;

reference_and_return:
    /* Reference the object as a part of the opening */
    oss_object_ref(obj);

    *pobj = obj;

    return 0;
}

error_t oss_open_connected_object_from_idx(u32 idx, struct oss_object* rel, struct oss_object** pobj, enum OSS_CONNECTION_DIRECTION dir)
{
    oss_object_t* obj = NULL;
    oss_connection_t* conn;

    /* Try and get a connection in the right direction */
    switch (dir) {
    case OSS_CONNECTION_DOWNSTREAM:
        conn = oss_object_get_connection_down_idx(rel, idx);
        break;
    case OSS_CONNECTION_UPSTREAM:
        conn = oss_object_get_connection_up_idx(rel, idx);
        break;
    default:
        /* First, check if there is a connection with this index already */
        conn = oss_object_get_connection_idx(rel, idx);
        break;
    }

    if (!conn)
        return -ENOENT;

    /* Grab the right object */
    obj = (oss_connection_is_downstream(conn, rel) ? conn->child : conn->parent);

    /* Reference this object as a result of the open action */
    oss_object_ref(obj);

    /* Export the thing */
    *pobj = obj;

    return 0;
}

/* Tries to remove an object from the oss graph */
error_t oss_try_remove_object(struct oss_object* obj)
{
    kernel_panic("TODO: Implement oss_try_remove_object");
}

const char* oss_get_endpoint_key(const char* path)
{
    const char* ret;
    oss_path_t oss_path;

    /* Parse the thing */
    if (oss_parse_path(path, &oss_path) && oss_path.n_subpath)
        return nullptr;

    /* Duplicate this string */
    ret = strdup(oss_path.subpath_vec[oss_path.n_subpath - 1]);

    /* Kill the path */
    oss_destroy_path(&oss_path);

    return ret;
}

error_t oss_connect_root_object(struct oss_object* object)
{
    error_t error;

    mutex_lock(oss_lock);

    /* Make sure the root objects have height zero */
    object->height = 0;

    /* Yeet it in the thing */
    error = hashmap_put(root_objects, (hashmap_key_t)object->key, object);

    mutex_unlock(oss_lock);

    return error;
}

/*!
 * @brief: Connect a new dir object with an fsroot to @parent
 *
 * If @parent is null, we'll create a new root object with the name @mountpoint
 */
error_t oss_connect_fsroot(struct oss_object* parent, const char* mountpoint, const char* fstype, struct volume* volume, struct oss_object** pobj)
{
    fs_type_t* type;
    oss_object_t* fsroot_object;

    if (!mountpoint || !fstype || !volume)
        return -EINVAL;

    /* Grab the wanted filesystem type */
    type = get_fs_type(fstype);

    /* Try to create a new root object */
    fsroot_object = fsroot_mount(parent, type, mountpoint, volume);

    if (!fsroot_object)
        return -ENODEV;

    /* Export the object if the caller wants it */
    if (pobj)
        *pobj = fsroot_object;

    return 0;
}

error_t oss_disconnect_fsroot(struct oss_object* connector, struct oss_object* fs)
{
    dir_t* dir;
    fs_root_object_t* fsroot;

    if (!fs)
        return -EINVAL;

    /* A fsroot object must always be a directory */
    dir = oss_object_unwrap(fs, OT_DIR);

    /* We can only call disconnect fsroot on the actual root dir */
    if (!dir || !dir->fsroot || (dir->flags & DIR_FSROOT) != DIR_FSROOT)
        return -EINVAL;

    fsroot = dir->fsroot;

    /* Unmount the fsroot object */
    return fsroot_unmount(connector, fsroot);
}

struct oss_object_ops* oss_get_generic_ops()
{
    return &__generic_oss_object_ops;
}

/*!
 * @brief: Create OSS root node registry
 */
void init_oss()
{
    oss_object_t* c_obj;

    /* Initialize the actual oss */
    init_oss_objects();
    init_oss_connections();

    /* Create supplementary objects */
    root_objects = create_hashmap(MAX_OSS_ROOT_OBJS, NULL);
    oss_lock = create_mutex(NULL);

    /* Create the root objects */
    for (u32 i = 0; i < arrlen(__root_object_keys); i++) {
        c_obj = create_oss_object(__root_object_keys[i], NULL, OT_GENERIC, &__generic_oss_object_ops, NULL);

        /* Register */
        oss_connect_root_object(c_obj);
    }
}
