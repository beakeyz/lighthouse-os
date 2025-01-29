#ifndef __ANIVA_OSS_OBJECT_H__
#define __ANIVA_OSS_OBJECT_H__

#include "libk/data/linkedlist.h"
#include "lightos/api/objects.h"
#include "oss/connection.h"
#include "sync/mutex.h"
#include <libk/string.h>

struct oss_object;

typedef struct oss_object_ops {
    /*
     * Functions
     */

    /*
     * Called when this object is created (optional)
     */
    int (*f_Create)(struct oss_object* this);

    /*
     * Called when this object is destroyed
     * NOTE: This doesn't directly destroy the oss object. It simply tells the
     * underlying subsystem that one of its objects is getting destroyed
     */
    int (*f_Destroy)(struct oss_object* this);

    /*
     * Connect an object to this object. This increases the reference
     * count of @obj, since it is now also referenced by @this
     */
    int (*f_Connect)(struct oss_object* this, struct oss_object* obj);
    /*
     * Connects a new object to this object. Oss drivers may implement
     * their own way to create and connect the new object
     */
    int (*f_ConnectNew)(struct oss_object* this, const char* key, enum OSS_OBJECT_TYPE type, struct oss_object** p_result);
    /*
     * Remove a connection with another object.
     * Removes the reference @this has over @obj.
     */
    int (*f_Disconnect)(struct oss_object* this, struct oss_object* obj);
    /*
     * Reads some data from an object. Reads propegate to all downstream
     * connections
     */
    ssize_t (*f_Read)(struct oss_object* this, u64 offset, void* buffer, size_t size);
    /*
     * Write some data to an object. Writes propegate to all downstream
     * connections
     */
    ssize_t (*f_Write)(struct oss_object* this, u64 offset, void* buffer, size_t size);
    /*
     * Tries to open an object connected to this object. Called when trying to
     * open an object that doesn't seem connected to the object we end up at
     */
    int (*f_Open)(struct oss_object* this, const char* key, struct oss_object** p_child);
    /*
     * Tries to open an object on @this using the index @idx
     */
    int (*f_OpenIdx)(struct oss_object* this, u32 idx, struct oss_object** p_child);
    /*
     * Flushes all object buffers to their backing stores
     * any cached shit is flushed down and removed.
     */
    int (*f_Flush)(struct oss_object* this);
    /*
     * Renames the objects backing entity. Useful for filesystem
     * drivers, which may need to update fs entry names
     */
    int (*f_Rename)(struct oss_object* this, const char* new_key);
    /*
     * Gets object info
     */
    int (*f_GetInfo)(struct oss_object* this, object_info_t* info_buffer);
    /*
     * Tries to remove this object from it's connections.
     * Fails if there are connections where this guy is the parent still
     * TODO: Check if this call can somehow have combined functionallity with f_Disconnect
     */
    // int (*f_Remove)(struct oss_object* this);
} oss_object_ops_t;

#define OSS_OBJECT_MAX_KEY_LEN 128

/* This is the value of ->height if it hasn't been set yet */
#define OSS_OBJECT_HEIGHT_NOTSET ((u32) - 1)

/*
 * Objects: The core of the oss
 *
 * Objects can be connected to each other with 'connections', which any object
 * can have multiple of, creating a sort of graph network
 */
typedef struct oss_object {
    /* Unique key that identifies */
    const char* key;

    /*
     * Number of references to this object. This guy is signed
     * because that saves us headache during ref and unref
     */
    i32 nr_references;
    /* Object flags */
    u32 flags;
    /*
     * Relative height of this object to the root. This is set the first time an object
     * is connected to another object
     */
    u32 height;
    /*
     * Object type. Truncated to a bitfield of 16 bits
     * TODO: Check if this doesn't fuck us
     */
    enum OSS_OBJECT_TYPE type;

    mutex_t* lock;

    /* Store a pointer to the ops field (which should just be kept staticly somewhere) */
    oss_object_ops_t* ops;
    /*
     * List of oss connections. This list may NEVER contain
     * connections with children or parents of the same name
     * TODO: Make this a hash map based on string key
     */
    list_t* connections;
    /* Private field */
    void* private;
} oss_object_t;

static inline void* oss_object_unwrap(oss_object_t* object, enum OSS_OBJECT_TYPE required_type)
{
    if (!object || object->type != required_type)
        return nullptr;

    return object->private;
}

static inline bool oss_object_implements_any_method(oss_object_t* object)
{
    oss_object_ops_t unimplemented_ops = { NULL };

    /* Check if all the fields inside object->ops are cleared. If that's not the case, this object implements at least one method */
    return (object && object->ops != NULL && (memcmp(object->ops, &unimplemented_ops, sizeof(unimplemented_ops)) == false));
}

static inline size_t oss_object_get_nr_connections(oss_object_t* object)
{
    return (object && object->connections) ? object->connections->m_length : 0;
}

static inline bool oss_object_can_set_type(oss_object_t* object)
{
    /*
     * Objects without a type can always have their type set. If
     * a generic object doesn't yet have a private field, it can also be
     * transformed.
     *
     * An object also can't be connected to any other object (either upstream or downstream) if
     * we want to transform it's type
     */
    return (object && oss_object_get_nr_connections(object) == 0 && (object->type == OT_NONE || object->type == OT_GENERIC) && NULL == object->private && !oss_object_implements_any_method(object));
}

oss_object_t* create_oss_object(const char* key, u16 flags, enum OSS_OBJECT_TYPE type, oss_object_ops_t* ops, void* private);

/* Take an object, increase the reference count */
error_t oss_object_ref(oss_object_t* object);
/* Release an object, decrease the reference count, maybe destroy the object */
error_t oss_object_unref(oss_object_t* object);
error_t oss_object_move_ref(oss_object_t* deref, oss_object_t* ref);

error_t oss_object_settype(oss_object_t* object, enum OSS_OBJECT_TYPE type, oss_object_t** presult);
error_t oss_object_rename(oss_object_t* object, const char* new_key);
error_t oss_object_close_connections(oss_object_t* object);
error_t oss_object_close_upstream_connections(oss_object_t* object);

oss_connection_t* oss_object_get_connection(oss_object_t* object, const char* key);
oss_connection_t* oss_object_get_connection_down(oss_object_t* object, const char* key);
oss_connection_t* oss_object_get_connection_up(oss_object_t* object, const char* key);
oss_connection_t* oss_object_get_connection_idx(oss_object_t* object, u32 idx);
oss_connection_t* oss_object_get_connection_up_idx(oss_object_t* object, u32 idx);
oss_connection_t* oss_object_get_connection_down_idx(oss_object_t* object, u32 idx);

oss_object_t* oss_object_get_connected(oss_object_t* object, const char* key);

/* Interface functions for talking with oss objects */
error_t oss_object_connect(oss_object_t* parent, oss_object_t* child);
error_t oss_object_connect_new(oss_object_t* parent, const char* key, enum OSS_OBJECT_TYPE type, oss_object_t** pobj);
error_t oss_object_disconnect(oss_object_t* parent, oss_object_t* child);
ssize_t oss_object_read(oss_object_t* this, u64 offset, void* buffer, size_t size);
ssize_t oss_object_write(oss_object_t* this, u64 offset, void* buffer, size_t size);
error_t oss_object_open(oss_object_t* this, const char* key, oss_object_t** pobj);
error_t oss_object_close(oss_object_t* this);
error_t oss_object_flush(oss_object_t* this);

error_t oss_object_get_abs_path(oss_object_t* object, char* buffer, size_t* p_bsize);

void init_oss_objects();

#endif // !__ANIVA_OSS_OBJECT_H__
