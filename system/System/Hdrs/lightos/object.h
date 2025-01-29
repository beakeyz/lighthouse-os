#ifndef __LIGHTOS_OBJECT_H__
#define __LIGHTOS_OBJECT_H__

#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include <lightos/types.h>

/* Control functions for getting access to objects */
/*!
 * @brief: Opens the object of the current process
 */
Object* OpenSelfObject(u32 flags);
/*!
 * @brief: Gets an object from a previously opened handle
 */
Object* GetObjectFromHandle(HANDLE handle);
/*!
 * @brief: Creates a new object
 *
 * This object is unconnected and we hold the only reference to it. Next thing to do is to connect
 * it to another object
 */
Object* CreateObject(const char* key, u16 flags, enum OSS_OBJECT_TYPE type);
Object* OpenObject(const char* path, u32 hndl_flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode);
Object* OpenObjectFrom(Object* rel, const char* path, u32 hndl_flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode);
Object* OpenObjectIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode);
Object* OpenConnectedObjectByIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode);
Object* OpenDownConnectedObjectByIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode);
Object* OpenUpConnectedObjectByIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode);
error_t CloseObject(Object* obj);

/*!
 * @brief: Opens the current working object
 *
 * This object should be attached to every process object to determine the
 * default place of operations for this process. Object searches can then
 * be done relative to the working object
 */
Object* OpenWorkingObject(u32 flags);
/*!
 * @brief: Changes the current working object
 *
 * Tries to set @object as the current working object. There can only be one
 * working object per process
 */
error_t ChWorkingObject(Object* object);

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

error_t ObjectGetInfo(Object* object, object_info_t* infobuff);

#endif // !__LIGHTOS_OBJECT_H__
