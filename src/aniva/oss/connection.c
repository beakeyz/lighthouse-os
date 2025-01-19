#include "connection.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "oss/object.h"
#include <mem/zalloc/zalloc.h>

static zone_allocator_t* connection_pool;

oss_connection_t* create_oss_connection(struct oss_object* parent, struct oss_object* child)
{
    oss_connection_t* ret;

    ret = zalloc_fixed(connection_pool);

    if (!ret)
        return nullptr;

    ret->parent = parent;
    ret->child = child;

    return ret;
}

void destroy_oss_connection(oss_connection_t* conn)
{
    zfree_fixed(connection_pool, conn);
}

/*!
 * @brief: Connects @child to @parent and vice versa
 *
 * Only @child gets a reference though, since we otherwise get too many references everywhere
 */
int oss_connect(oss_object_t* parent, oss_object_t* child)
{
    oss_connection_t* conn;

    /*
     * First, check if we already have a connection with this child, since
     * we can't have duplicate connections
     */
    if (oss_object_get_connection(parent, child->key) != nullptr)
        return -EDUPLICATE;

    conn = create_oss_connection(parent, child);

    if (!conn)
        return -ENOMEM;

    /* Add the connections to the objects */
    list_append(parent->connections, conn);
    list_append(child->connections, conn);

    /* Give the parent a reference to the child */
    oss_object_ref(child);

    return 0;
}

int oss_disconnect(oss_connection_t* conn)
{
    oss_object_t* parent;
    oss_object_t* child;
    oss_connection_t* target = nullptr;

    if (!conn || !conn->parent || !conn->child)
        return -EINVAL;

    parent = conn->parent;
    child = conn->child;

    FOREACH(i, parent->connections)
    {
        /* Check if this is our connection */
        if (oss_connection_equals(i->data, conn)) {
            target = i->data;
            break;
        }
    }

    /* No connection on the parent that matches this */
    if (!target)
        return -ENOENT;

    /* Remove the connections from the two objects */
    list_remove_ex(child->connections, target);
    list_remove_ex(parent->connections, target);

    /* Release the reference @parent still has on @child */
    oss_object_unref(child);

    /* Kill the connection memory */
    destroy_oss_connection(target);

    return 0;
}

void init_oss_connections()
{
    connection_pool = create_zone_allocator(16 * Kib, sizeof(oss_connection_t), NULL);

    ASSERT(connection_pool);
}
