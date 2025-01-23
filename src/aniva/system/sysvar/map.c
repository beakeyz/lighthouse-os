#include "map.h"
#include "libk/data/linkedlist.h"
#include "lightos/api/objects.h"
#include "mem/heap.h"
#include "oss/connection.h"
#include "oss/core.h"
#include "oss/object.h"
#include "system/profile/attr.h"
#include "system/sysvar/var.h"

bool oss_object_can_contain_sysvar(oss_object_t* node)
{
    return (node->type == OT_PROCESS || node->type == OT_PROFILE || node->type == OT_GENERIC);
}

/*!
 * @brief: Grab a sysvar from a node
 *
 * Try to find a sysvar at @key inside @node
 */
sysvar_t* sysvar_get(oss_object_t* object, const char* key)
{
    sysvar_t* ret;
    oss_object_t* obj;

    if (!object || !key)
        return nullptr;

    /*
     * Make sure to use the object function, which doesn't touch the reference
     */
    if (oss_object_open(object, key, &obj))
        return nullptr;

    /* Verify that the object is actually valid */
    if (!obj || obj->type != OT_SYSVAR || !obj->private)
        return nullptr;

    /* Grab the private field */
    ret = obj->private;

    /* Return with a taken reference */
    return get_sysvar(ret);
}

static size_t __get_nr_sysvar_objects(oss_object_t* rel)
{
    oss_connection_t* conn;
    size_t nr = 0;

    FOREACH(i, rel->connections)
    {
        conn = i->data;

        /* Skip upstream connections */
        if (oss_connection_is_upstream(conn, rel))
            continue;

        if (conn->child->type == OT_SYSVAR)
            nr++;
    }

    return nr;
}

/*!
 * @brief: Dump all the variables on @node
 *
 * Leaks weak references to the variables. This function expects @node to be locked
 */
int sysvar_dump(struct oss_object* node, struct sysvar*** barr, size_t* bsize)
{
    sysvar_t** var_arr;

    if (!node || !barr || !bsize)
        return -1;

    /* First, count the variables on this node */
    *bsize = __get_nr_sysvar_objects(node);

    if (!(*bsize))
        return -KERR_NOT_FOUND;

    /* Allocate the list */
    var_arr = kmalloc(sizeof(sysvar_t*) * (*bsize));

    if (!(*barr))
        return -KERR_NOMEM;

    size_t nr = 0;
    oss_connection_t* conn;

    FOREACH(i, node->connections)
    {
        conn = i->data;

        /* Skip upstream connections */
        if (oss_connection_is_upstream(conn, node))
            continue;

        if (conn->child->type == OT_SYSVAR)
            var_arr[nr++] = conn->child->private;
    }

    /* Set the param */
    *barr = var_arr;
    return 0;
}

int sysvar_attach(struct oss_object* node, struct sysvar* var)
{
    error_t error;

    if (!var)
        return -EINVAL;

    /* Only these types may have sysvars */
    if (!oss_object_can_contain_sysvar(node))
        return -EINVAL;

    error = oss_object_connect(node, var->object);

    if (error)
        return error;

    /* Make sure the var knows what it's original parent is */
    if (!var->parent)
        var->parent = node;

    return error;
}

/*!
 * @brief: Attach a new sysvar into @node
 *
 *
 */
int sysvar_attach_ex(struct oss_object* node, const char* key, enum PROFILE_TYPE ptype, enum SYSVAR_TYPE type, uint8_t flags, void* buffer, size_t bsize)
{
    error_t error;
    sysvar_t* target;

    /*
     * Initialize a permission attributes struct for a generic sysvar. By default
     * sysvars can be seen and read from by everyone.
     */
    pattr_t pattr = {
        .ptype = ptype,
        .pflags = PATTR_UNIFORM_FLAGS(PATTR_SEE | PATTR_READ),
    };

    /* Make sure anyone from the same level can also mutate this fucker */
    pattr.pflags[ptype] |= (PATTR_WRITE | PATTR_DELETE);

    /* Try to create this fucker */
    target = create_sysvar(key, &pattr, type, flags, buffer, bsize);

    if (!target)
        return -ENOMEM;

    error = sysvar_attach(node, target);

    /* This is kinda yikes lol */
    if (error)
        release_sysvar(target);

    return error;
}

/*!
 * @brief: Remove a sysvar through its oss obj
 *
 * NOTE: Right now this also releases the reference the caller has
 */
int sysvar_detach(struct oss_object* node, const char* key, struct sysvar** bvar)
{
    sysvar_t* var;
    oss_object_t* obj;

    if (oss_open_object_from(key, node, &obj))
        return -ENOENT;

    /* Immediately lose the reference lol */
    oss_object_close(obj);

    if (obj->type != OT_SYSVAR)
        return -EINVAL;

    /* At this point this call should never fail */
    ASSERT_MSG(oss_object_disconnect(node, obj) == 0, "oss_object_disconnect failed in sysvar_detach");

    var = obj->private;

    sysvar_lock(var);

    /* Only return the reference if this variable is still referenced */
    if (var->object->nr_references > 1 && bvar)
        *bvar = var;

    if (var->parent == node) {
        /* Check if there is another upstream connection that becomes the parent now */
        oss_connection_t* conn = oss_object_get_connection_up_idx(var->object, 0);

        var->parent = (!conn) ? nullptr : conn->parent;
    }

    sysvar_unlock(var);

    /* Release a reference */
    release_sysvar(var);

    return 0;
}
