#ifndef __LIGHTOS_OBJECT_H__
#define __LIGHTOS_OBJECT_H__

#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include <lightos/types.h>

/* Control functions for getting access to objects */
Object* CreateObject(const char* key, u16 flags, enum OSS_OBJECT_TYPE type);
Object* CreateObjectFromHandle(HANDLE handle);
Object* OpenObject(const char* path, u32 hndl_flags, enum HNDL_MODE mode);
Object* OpenObjectFrom(Object* rel, const char* path, u32 hndl_flags, enum HNDL_MODE mode);
Object* OpenObjectFromIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode);
Object* OpenConnectedObjectFromIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode);
error_t CloseObject(Object* obj);

/*!
 * @brief: Checks if an object is actually valid
 */
bool ObjectIsValid(Object* obj);

static inline Object InvalidObject()
{
    return (Object) {
        .handle = HNDL_INVAL,
        .type = OT_INVALID,
        .key = "__inval__"
    };
}

/*!
 * @brief: Attempts to set the type of @obj to a new type
 *
 * Only works if @obj is a generic and unconnected object
 */
error_t ObjectSetType(Object* obj, enum OSS_OBJECT_TYPE newtype);

/*!
 * @brief: Read from an object
 *
 * @returns: The size that was read on succes, negative error code otherwise
 */
ssize_t ObjectRead(Object* obj, u64 offset, void* buffer, size_t size);

/*!
 * @brief: Write to an object
 * @returns: The size that was written on succes, negative error code otherwise
 */
ssize_t ObjectWrite(Object* obj, u64 offset, void* buffer, size_t size);

/*!
 * @brief: Tries to send a message to the underlying object type
 */
error_t ObjectMessage(Object* obj, u64 msgc, u64 offset, void* buffer, size_t size);

/*!
 * @brief: Connect @child to @obj
 */
error_t ObjectConnect(Object* obj, Object* child);

/*!
 * @brief: Disconnect @child from @obj
 */
error_t ObjectDisconnect(Object* obj, Object* child);

#endif // !__LIGHTOS_OBJECT_H__
