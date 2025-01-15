#ifndef __LIGHTOS_OBJECT_H__
#define __LIGHTOS_OBJECT_H__

#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include <lightos/types.h>

typedef struct object {
    /* This is the key this object is identified with */
    const char* key;
    /* Handle to the kernel object */
    HANDLE handle;
    /* Type of this object */
    enum OSS_OBJECT_TYPE type;
} Object, object_t;

/* Control functions for getting access to objects */
Object CreateObject(const char* key, u16 flags, enum OSS_OBJECT_TYPE type);
Object OpenObject(const char* key);
error_t CloseObject(Object* obj);

/*!
 * @brief: Checks if an object is actually valid
 */
bool ObjectIsValid(Object* obj);

/*!
 * @brief: Read from an object
 */
error_t ObjectRead(Object* obj, u64 offset, void* buffer, size_t size);

/*!
 * @brief: Write to an object
 */
error_t ObjectWrite(Object* obj, u64 offset, void* buffer, size_t size);

/*!
 * @brief: Connect @child to @obj
 */
error_t ObjectConnect(Object* obj, Object* child);

/*!
 * @brief: Disconnect @child from @obj
 */
error_t ObjectDisconnect(Object* obj, Object* child);

#endif // !__LIGHTOS_OBJECT_H__
