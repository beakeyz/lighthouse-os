#include "core.h"
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

static const char* __root_object_keys[] = {
    "Storage",
    "Devices",
    "Drivers",
    "Terminals",
    "Proc",
    "Runtime",
};

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
 * @brief: Recursively open an object relative to a starting object
 *
 * NOTE: This function doesn't check parameters, so callers should do that
 * We simply walk all the connections relative to @rel. If there isn't a connection for a
 * certain object, we try to create one using the f_Open call on the object. If that fails,
 * we assume we've hit a dead end and we bail
 */
static error_t __oss_open_object_from(oss_path_t* path, u32 start_idx, struct oss_object* rel, struct oss_object** pobj)
{
    u32 i = start_idx;
    oss_object_t* walker = rel;

    /* Walk the object graph */
    do {
        if (oss_object_open(walker, path->subpath_vec[i], &walker))
            return -ENOENT;

        i++;
    } while (walker && i < path->n_subpath);

    /* We need to still have a walker at the end of this */
    if (!walker || i != path->n_subpath)
        return -ENOENT;

    /* We've got an object! We do need to reference it lol */
    oss_object_ref(walker);

    /* Export the guy */
    *pobj = walker;

    return 0;
}

error_t oss_open_object(const char* path, struct oss_object** pobj)
{
    error_t error;
    oss_object_t* root_obj;
    oss_path_t oss_path;

    if (!path || !pobj)
        return -EINVAL;

    if (HAS_ERROR(oss_parse_path(path, &oss_path)))
        return -EINVAL;

    /* Try and grab a root node */
    error = oss_open_root_object(path, &root_obj);

    if (error)
        goto destroy_path_and_exit;

    /* Try to open the object relative to the root object we just found */
    error = __oss_open_object_from(&oss_path, 1, root_obj, pobj);

destroy_path_and_exit:
    oss_destroy_path(&oss_path);

    return error;
}

error_t oss_open_object_from(const char* path, struct oss_object* rel, struct oss_object** pobj)
{
    error_t error;
    oss_path_t oss_path;

    if (!rel || !pobj)
        return -EINVAL;

    /* Parse the provided path */
    if (oss_parse_path(path, &oss_path))
        return -EINVAL;

    /* Try to find the object */
    error = __oss_open_object_from(&oss_path, 0, rel, pobj);

    /* Kill the path */
    oss_destroy_path(&oss_path);

    return error;
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

    /* Yeet it in the thing */
    error = hashmap_put(root_objects, (hashmap_key_t)object->key, object);

    mutex_unlock(oss_lock);

    return error;
}

static oss_object_ops_t root_object_ops = {
    NULL
};

/*!
 * @brief: Create OSS root node registry
 */
void init_oss()
{
    oss_object_t* c_obj;

    root_objects = create_hashmap(MAX_OSS_ROOT_OBJS, NULL);
    oss_lock = create_mutex(NULL);

    for (u32 i = 0; i < arrlen(__root_object_keys); i++) {
        c_obj = create_oss_object(__root_object_keys[i], NULL, OT_GENERIC, &root_object_ops, NULL);

        /* Register  */
        hashmap_put(root_objects, (hashmap_key_t)__root_object_keys[i], c_obj);
    }
}
