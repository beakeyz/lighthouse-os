#ifndef __ANIVA_OSS_OBJECT_H__
#define __ANIVA_OSS_OBJECT_H__

#include "libk/data/linkedlist.h"
#include "lightos/api/objects.h"
#include "oss/connection.h"
#include "sync/mutex.h"

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
    int (*f_ConnectNew)(struct oss_object* this, const char* key, enum OSS_OBJECT_TYPE type);
    /*
     * Remove a connection with another object.
     * Removes the reference @this has over @obj.
     */
    int (*f_Disconnect)(struct oss_object* this, struct oss_object* obj);
    /*
     * Reads some data from an object. Reads propegate to all downstream
     * connections
     */
    int (*f_Read)(struct oss_object* this, u64 offset, void* buffer, size_t size);
    /*
     * Write some data to an object. Writes propegate to all downstream
     * connections
     */
    int (*f_Write)(struct oss_object* this, u64 offset, void* buffer, size_t size);
    /*
     * Tries to open an object connected to this object. Called when trying to
     * open an object that doesn't seem connected to the object we end up at
     */
    int (*f_Open)(struct oss_object* this, const char* key, struct oss_object** p_child);
    /*
     * Flushes all object buffers to their backing stores
     * any cached shit is flushed down and removed.
     */
    int (*f_Flush)(struct oss_object* this);
    /*
     * Tries to remove this object from it's connections.
     * Fails if there are connections where this guy is the parent still
     * TODO: Check if this call can somehow have combined functionallity with f_Disconnect
     */
    // int (*f_Remove)(struct oss_object* this);
} oss_object_ops_t;

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
    u16 flags;
    /*
     * Object type. Truncated to a bitfield of 16 bits
     * TODO: Check if this doesn't fuck us
     */
    enum OSS_OBJECT_TYPE type : 16;

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

oss_object_t* create_oss_object(const char* key, u16 flags, enum OSS_OBJECT_TYPE type, oss_object_ops_t* ops, void* private);

/* Take an object, increase the reference count */
error_t oss_object_ref(oss_object_t* object);
/* Release an object, decrease the reference count, maybe destroy the object */
error_t oss_object_unref(oss_object_t* object);

oss_connection_t* oss_object_get_connection(oss_object_t* object, const char* key);
oss_connection_t* oss_object_get_connection_down(oss_object_t* object, const char* key);
oss_connection_t* oss_object_get_connection_up(oss_object_t* object, const char* key);
oss_connection_t* oss_object_get_connection_up_nr(oss_object_t* object, u32 idx);

/* Interface functions for talking with oss objects */
error_t oss_object_connect(oss_object_t* parent, oss_object_t* child);
error_t oss_object_connect_new(oss_object_t* parent, const char* key, enum OSS_OBJECT_TYPE type);
error_t oss_object_disconnect(oss_object_t* parent, oss_object_t* child);
error_t oss_object_read(oss_object_t* this, u64 offset, void* buffer, size_t size);
error_t oss_object_write(oss_object_t* this, u64 offset, void* buffer, size_t size);
error_t oss_object_open(oss_object_t* this, const char* key, oss_object_t** pobj);
error_t oss_object_close(oss_object_t* this);
error_t oss_object_flush(oss_object_t* this);

void init_oss_objects();

#endif // !__ANIVA_OSS_OBJECT_H__
