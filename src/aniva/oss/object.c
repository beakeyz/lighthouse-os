#include "object.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/api/objects.h"
#include "mem/zalloc/zalloc.h"
#include "oss/connection.h"
#include "sync/mutex.h"
#include <libk/string.h>
#include <mem/heap.h>

static zone_allocator_t* object_pool;

static int __oss_object_create(oss_object_t* obj)
{
    if (!obj->ops || !obj->ops->f_Create)
        return -EINVAL;

    return obj->ops->f_Create(obj);
}

static int __oss_object_destroy(oss_object_t* obj)
{
    node_t *c_conn_node, *next_conn_node;
    oss_connection_t* c_conn;

    /* First, flush the objects buffers */
    oss_object_flush(obj);

    /* Then, call the destructor */
    if (obj->ops->f_Destroy)
        obj->ops->f_Destroy(obj);

    /* Grab the  */
    c_conn_node = obj->connections->head;

    /* Kill any connections */
    while (c_conn_node) {
        /* Grab the next node preemptively */
        next_conn_node = c_conn_node->next;
        /* Grab the connection this node holds */
        c_conn = c_conn_node->data;

        /* Ensure this object had valid connections */
        ASSERT_MSG(c_conn->parent != obj && c_conn->child == obj, "Tried to destroy an object which still has children");

        /* Kill this connection */
        oss_disconnect(c_conn);

        /* Cycle to the next node */
        c_conn_node = next_conn_node;
    }

    /* Kill the lock */
    destroy_mutex(obj->lock);

    /* Kill the list */
    destroy_list(obj->connections);

    /* Kill the copied key variable */
    kfree((void*)obj->key);

    /* Free the object memory */
    zfree_fixed(object_pool, obj);
    return 0;
}

oss_object_t* create_oss_object(const char* key, u16 flags, enum OSS_OBJECT_TYPE type, oss_object_ops_t* ops, void* private)
{
    oss_object_t* obj;

    if (!key || !ops)
        return nullptr;

    obj = zalloc_fixed(object_pool);

    if (!obj)
        return nullptr;

    memset(obj, 0, sizeof(oss_object_t));

    obj->key = strdup(key);
    obj->connections = init_list();
    obj->lock = create_mutex(NULL);
    obj->nr_references = 1;
    obj->ops = ops;
    obj->type = type;
    obj->flags = flags;
    obj->private = private;

    /*
     * Call the create method on this object
     * TODO: Should we care if this succeeds?
     */
    (void)__oss_object_create(obj);

    return obj;
}

/* Take an object, increase the reference count */
error_t oss_object_ref(oss_object_t* object)
{
    object->nr_references++;

    return 0;
}

/* Release an object, decrease the reference count, maybe destroy the object */
error_t oss_object_unref(oss_object_t* object)
{
    object->nr_references--;

    if (object->nr_references <= 0)
        return __oss_object_destroy(object);

    return 0;
}

/*
 * Marco to get a connection from an object, based on a condition
 * and a key
 */
#define __OSS_OBJECT_GET_CONN(object, conn, key, condition)              \
    size_t key_len = strlen(key);                                        \
    oss_connection_t* conn;                                              \
                                                                         \
    FOREACH(i, object->connections)                                      \
    {                                                                    \
        conn = i->data;                                                  \
        if ((condition) && strncmp(conn->child->key, key, key_len) == 0) \
            return conn;                                                 \
    }                                                                    \
    return nullptr

oss_connection_t* oss_object_get_connection(oss_object_t* object, const char* key)
{
    /* Just get all the connections on this object (unfiltered) */
    __OSS_OBJECT_GET_CONN(object, conn, key, true);
}

oss_connection_t* oss_object_get_connection_down(oss_object_t* object, const char* key)
{
    /* Get all the connections where @object is the parent, in order to get all the downstream connections with this object */
    __OSS_OBJECT_GET_CONN(object, conn, key, (conn->parent == object));
}

oss_connection_t* oss_object_get_connection_up(oss_object_t* object, const char* key)
{
    /* Get all the connections where @object is the child, in order to get all the upstream connections with this object */
    __OSS_OBJECT_GET_CONN(object, conn, key, (conn->child == object));
}

/*!
 * @brief: Does the actual work regarding object connecting
 */
static inline error_t __oss_object_connect(oss_object_t* parent, oss_object_t* child)
{
    /* There really isn't much more to it. Maybe in the future we can put some pre-processing here? */
    return oss_connect(parent, child);
}

/*!
 * @brief: Connects two objects together
 *
 * If the parent has f_Connect implemented, that is called first. If that call fails, we
 * won't try to connect the object anyway
 */
error_t oss_object_connect(oss_object_t* parent, oss_object_t* child)
{
    error_t error = EOK;

    /* Maybe call f_Connect */
    if (parent->ops->f_Connect)
        error = parent->ops->f_Connect(parent, child);

    /* Check if the f_Connect call might have failed */
    if (error)
        return error;

    /* Call the function to actually connect @child to @parent */
    return __oss_object_connect(parent, child);
}

static inline bool __oss_object_has_other_upstream_connections(oss_object_t* child, oss_object_t* parent)
{
    oss_connection_t* c_child_conn;

    /*
     * Walk the connection array. If we find a connection where @child is the child
     * and @parent is NOT the parent, it means @child has other upstream connections
     * other than the one with @parent
     */
    FOREACH(i, child->connections)
    {
        c_child_conn = i->data;

        if (c_child_conn->child == child && c_child_conn->parent != parent)
            return true;
    }

    return false;
}

/*!
 * @brief: Checks if @child can be disconnected from @parent and does the needed prefetching
 */
static inline error_t __oss_object_prep_disconnect(oss_object_t* parent, oss_object_t* child, oss_connection_t** p_parent_conn)
{
    oss_connection_t* parent_conn;

    /* If the child is still referenced externally, we for sure can't disconnect it */
    if (child->nr_references > 1)
        return -EINVAL;

    /* Grab the downstream connection on the parent */
    parent_conn = oss_object_get_connection_down(parent, child->key);

    if (!parent_conn)
        return -EINVAL;

    /* Check if we're doing a legal disconnect */
    if (!__oss_object_has_other_upstream_connections(child, parent))
        return -EINVAL;

    *p_parent_conn = parent_conn;

    return 0;
}

/*!
 * @brief: Try to disconnect an object from it's parent
 *
 * This checks to see if @parent has a downstream connection with @child. If it does,
 * we're going to check if @child has any other upstream connections if it still has connections left
 * after this disconnection. If both these conditions are
 * met, we can proceed with the disconnection. If the latter condition isn't met, it means
 * we're trying to disconnect an object that has no parent, but does have children. This is an unsafe
 * disconnect, since we'll lose the references to the children and the call will fail
 */
error_t oss_object_disconnect(oss_object_t* parent, oss_object_t* child)
{
    error_t error;
    oss_connection_t* parent_conn;

    /* Try to prepare for this disconnect */
    error = __oss_object_prep_disconnect(parent, child, &parent_conn);

    /* Illegal disconnect. Just dip */
    if (error)
        return error;

    /* Call the parents disconnect routine if it has one */
    if (parent->ops->f_Disconnect)
        error = parent->ops->f_Disconnect(parent, child);

    /* Call the pertaining disconnection function for the subsystem */
    return oss_disconnect(parent_conn);
}

/*!
 * @brief: Call the object read routine
 *
 * TODO: Check if we can implement some subsystem stuff with buffer juggling or some shit
 */
error_t oss_object_read(oss_object_t* this, u64 offset, void* buffer, size_t size)
{
    /* Check if the parameters are valid */
    if (!this || !buffer || !size)
        return -EINVAL;

    /* This object has no read function =( just dip */
    if (!this->ops->f_Read)
        return -ENOIMPL;

    /* Call the object function */
    return this->ops->f_Read(this, offset, buffer, size);
}

/*!
 * @brief: Call the object write routine
 *
 * This is a function that may propegate down the connection stream
 */
error_t oss_object_write(oss_object_t* this, u64 offset, void* buffer, size_t size)
{
    oss_connection_t* conn;
    error_t error;

    /* Check if the parameters are valid */
    if (!this || !buffer || !size)
        return -EINVAL;

    if (!this->ops->f_Write)
        return -ENOIMPL;

    /* Call the write routine on this object */
    error = this->ops->f_Write(this, offset, buffer, size);

    /* If this call failed, just dip */
    if (error)
        return error;

    FOREACH(i, this->connections)
    {
        conn = i->data;

        /* Skip upstream connections */
        if (conn->parent != this)
            continue;

        /* Skip objects without the propegate flag */
        if ((conn->child->flags & OF_PROPEGATE) != OF_PROPEGATE)
            continue;

        /* Call the write routine on this object */
        error = oss_object_write(conn->child, offset, buffer, size);

        /* If any write call fails, just dip */
        if (error)
            return error;
    }

    return 0;
}

/*!
 * @brief: Try to open an upstream object
 *
 * Since objects can be connected in any arbitrary way, we need to be able to open object
 * both 'upstream' and 'downstream' in the connections. This means we need to look for connections
 * where the target object is the child in the connection, when going downstream, but when going
 * upstream, the target object is the parent in the connection.
 *
 * Most objects are going to have one upstream connection though; their parent. We make an exception for
 * opening objects upstream, when there is only one connection. This connection can then be opened
 * with the special '..' key. If there are multiple upstream objects, this function requires the right
 * key for a given upstream object to be appended after the special upstream marker. An example path
 * might look like this:
 * 1) "Some/Object/.." -> This references the same path as "Some/Object"
 * 2) "Some/Other/Object/..Different" -> This references the upstream object with the key 'Different'.
 *    another path like the following may then exists: "A/Different/Object", which references the same
 *    object as the former path.
 *
 * NOTE: This routine assumes that the '..' key is already consumed by the caller
 */
static error_t __oss_object_open_upstream(oss_object_t* this, const char* key, oss_object_t** pobj)
{
    oss_connection_t* alternate_ret = nullptr;
    oss_connection_t* c_conn;
    size_t nr_upstream = 0;
    size_t key_len = strlen(key);

    FOREACH(i, this->connections)
    {
        c_conn = i->data;

        /* Skip downstream connections */
        if (c_conn->parent == this)
            continue;

        /* Count the number of upstream connections we find */
        nr_upstream++;

        /*
         * Set the alternate_ret if it hasn't been set yet. This is going to be used
         * in case we don't find a matching connection with @key and nr_upstream stays
         * one. In this case we can be sure that alternate_ret will be the only upstream
         * connection @this has
         */
        if (!alternate_ret)
            alternate_ret = c_conn;

        /* Check the key */
        if (strncmp(key, c_conn->parent->key, key_len) == 0) {
            /* Found it, export */
            *pobj = c_conn->parent;
            return 0;
        }
    }

    /* This really shouldn't happen lol */
    if (!nr_upstream || !alternate_ret)
        return -EINVAL;

    /*
     * We also require only '..' to be provided here. Otherwise
     * we do want to ask the object itself if it knows an upstream
     * object lol
     */
    if (nr_upstream > 1 || key[0] != '\0')
        return -ENOENT;

    *pobj = alternate_ret->parent;
    return 0;
}

/*!
 * @brief: Open a (downstream) object connected to @this
 *
 * There are a few special entries for @key:
 * 1) '.': This just returns @this
 * 2) '..*': This searches for upstream objects in stead of downstream
 *
 * NOTE: THIS FUNCTION DOES NOT REFERENCE THE OPENED OBJECT
 */
error_t oss_object_open(oss_object_t* this, const char* key, oss_object_t** pobj)
{
    oss_object_t* ret = nullptr;
    oss_connection_t* c_conn;

    if (!this || !key || !pobj)
        return -EINVAL;

    if (key[0] == '.' && key[1] == '\0') {
        ret = this;
        goto reference_and_return;
    }

    /* Try to get a connection based on the up or downstream requirements */
    if (strncmp(key, "..", 2) == 0) {
        /* Try to open upstream if that was requested. Also skip the two dot chars '..' */
        if (__oss_object_open_upstream(this, key + 2, &ret))
            /* Really make sure ret is null if this call failed */
            ret = nullptr;
        /* Otherwise simply try to get a downstream connection */
    } else if ((c_conn = oss_object_get_connection_down(this, key)) != nullptr)
        ret = c_conn->child;

    /* Connection found, we can return */
    if (ret)
        goto reference_and_return;

    /* Do we have a way for the object to open it's own object? */
    if (!this->ops->f_Open)
        return -ENOENT;

    /* Yes! Let's try it... */
    if (this->ops->f_Open(this, key, &ret))
        return -ENOENT;

reference_and_return:

    *pobj = ret;

    return 0;
}

error_t oss_object_close(oss_object_t* this)
{
    return oss_object_unref(this);
}

error_t oss_object_flush(oss_object_t* this)
{
    if (!this || !this->ops->f_Flush)
        return -EINVAL;

    return this->ops->f_Flush(this);
}

void init_oss_objects()
{
    object_pool = create_zone_allocator(32 * Kib, sizeof(oss_object_t), NULL);

    ASSERT(object_pool);
}
