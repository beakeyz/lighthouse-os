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

    /* Set the connection gradient */
    if (child->height == OSS_OBJECT_HEIGHT_NOTSET) {
        /* Child has no height set yet.  */
        ret->gradient = 1;
        child->height = parent->height + 1;
    } else
        /* If the child does have a set height, compute the gradient of this connection */
        ret->gradient = child->height - parent->height;

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

    if (!parent || !child)
        return -EINVAL;

    /* Can't connect an object to itself */
    if (parent == child)
        return -EINVAL;
    /*
     * First, check if we already have a connection with this child, since
     * we can't have duplicate connections
     */
    if (oss_object_get_connection(parent, child->key) != nullptr)
        return -EDUPLICATE;

    if (oss_object_get_connection(child, parent->key) != nullptr)
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

    if (!conn || !conn->parent || !conn->child)
        return -EINVAL;

    parent = conn->parent;
    child = conn->child;

    /* Remove the connections from the two objects */
    list_remove_ex(child->connections, conn);
    list_remove_ex(parent->connections, conn);

    /* Release the reference @parent still has on @child */
    oss_object_close(child);

    /* Kill the connection memory */
    destroy_oss_connection(conn);

    return 0;
}

void init_oss_connections()
{
    connection_pool = create_zone_allocator(16 * Kib, sizeof(oss_connection_t), NULL);

    ASSERT(connection_pool);
}
