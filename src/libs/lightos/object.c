#include "object.h"
#include "lightos/api/handle.h"
#include "lightos/handle.h"
#include "lightos/syscall.h"
#include "stdlib.h"
#include <lightos/api/objects.h>
#include <string.h>

static enum OSS_OBJECT_TYPE __get_object_type(HANDLE handle)
{
    return sys_get_object_type(handle);
}

static enum OSS_OBJECT_TYPE __set_and_get_object_type(HANDLE handle, enum OSS_OBJECT_TYPE type)
{
    return sys_set_object_type(handle, type);
}

static void __get_object_key(const char* path, char* kbuff, size_t kbuff_len)
{
    const char* ret = path + strlen(path);

    /* Find the last slash slash  */
    do {
        ret--;
    } while (ret != path && *ret != '/');

    /* Since ret is now pointing at a '/', we need to add one, in order to point to the actual key */
    strncpy(kbuff, ret + 1, kbuff_len);
}

Object CreateObject(const char* key, u16 flags, enum OSS_OBJECT_TYPE type)
{
    Object ret = { 0 };

    ret.handle = open_handle(key, HNDL_TYPE_OBJECT, HNDL_FLAG_RW, HNDL_MODE_CREATE_NEW);

    /* If the handle isn't valid, we can just dip */
    if (handle_verify(ret.handle))
        return ret;

    /* Set the object type */
    ret.type = __set_and_get_object_type(ret.handle, type);

    /* Copy the key */
    strncpy((char*)&ret.key[0], key, sizeof(ret.key));

    return ret;
}

Object OpenObject(const char* path, u32 hndl_flags, enum HNDL_MODE mode)
{
    return OpenObjectFrom(nullptr, path, hndl_flags, mode);
}

Object OpenObjectFrom(Object* rel, const char* path, u32 hndl_flags, enum HNDL_MODE mode)
{
    Object ret = { 0 };

    if (rel)
        ret.handle = open_handle_from(rel->handle, path, HNDL_TYPE_OBJECT, hndl_flags, mode);
    else
        ret.handle = open_handle(path, HNDL_TYPE_OBJECT, hndl_flags, mode);

    /* If the handle isn't valid, we can just dip */
    if (handle_verify(ret.handle))
        return ret;

    /* Ask the kernel what the type of this object is */
    ret.type = __get_object_type(ret.handle);

    /*
     * Get the object key from the path.
     * We know the key of an object is always the last entry
     * in a path, so we can just scan the path from the back to
     * find the last path entry
     */
    __get_object_key(path, (char*)&ret.key[0], sizeof(ret.key));

    return ret;
}

error_t CloseObject(Object* obj)
{
    if (close_handle(obj->handle))
        return -EINVAL;

    obj->type = OT_NONE;
    obj->handle = HNDL_INVAL;

    return 0;
}

bool ObjectIsValid(Object* obj)
{
    return (obj && obj->type && handle_verify(obj->handle) == 0);
}

error_t ObjectRead(Object* obj, u64 offset, void* buffer, size_t size)
{
    if (!ObjectIsValid(obj))
        return -EINVAL;

    return handle_read(obj->handle, offset, buffer, size);
}

error_t ObjectWrite(Object* obj, u64 offset, void* buffer, size_t size)
{
    if (!ObjectIsValid(obj))
        return -EINVAL;

    return handle_write(obj->handle, offset, buffer, size);
}

error_t ObjectMessage(Object* obj, u64 msgc, u64 offset, void* buffer, size_t size)
{
    if (!ObjectIsValid(obj))
        return -EINVAL;

    return sys_send_msg(obj->handle, msgc, offset, buffer, size);
}

error_t ObjectConnect(Object* obj, Object* child)
{
    exit_noimpl("ObjectConnect");
    return 0;
}

error_t ObjectDisconnect(Object* obj, Object* child)
{
    exit_noimpl("ObjectDisconnect");
    return 0;
}
