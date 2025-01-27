#include "object.h"
#include "lightos/api/handle.h"
#include "lightos/handle.h"
#include "lightos/syscall.h"
#include "stdlib.h"
#include <lightos/api/objects.h>
#include <stdio.h>
#include <string.h>

static enum OSS_OBJECT_TYPE __get_object_type(HANDLE handle)
{
    return sys_get_object_type(handle);
}

static enum OSS_OBJECT_TYPE __set_and_get_object_type(HANDLE handle, enum OSS_OBJECT_TYPE type)
{
    return sys_set_object_type(handle, type);
}

static void __get_object_key(Object* object)
{
    (void)sys_get_object_key(object->handle, object->key, sizeof(object->key));
}

Object* CreateObject(const char* key, u16 flags, enum OSS_OBJECT_TYPE type)
{
    Object* ret;

    if (!key)
        return nullptr;

    ret = malloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->handle = open_handle(key, HNDL_TYPE_OBJECT, HNDL_FLAG_RW, HNDL_MODE_CREATE_NEW);

    /* If the handle isn't valid, we can just dip */
    if (handle_verify(ret->handle)) {
        free(ret);
        return nullptr;
    }

    /* Set the object type */
    ret->type = __set_and_get_object_type(ret->handle, type);

    /* Copy the key */
    strncpy((char*)&ret->key[0], key, sizeof(ret->key));

    return ret;
}

Object* OpenObject(const char* path, u32 hndl_flags, enum HNDL_MODE mode)
{
    return OpenObjectFrom(nullptr, path, hndl_flags, mode);
}

static Object* __open_object(HANDLE handle)
{
    Object* ret;

    ret = malloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    ret->handle = handle;
    /* Ask the kernel what the type of this object is */
    ret->type = __get_object_type(handle);

    /*
     * Get the object key from the path.
     * We know the key of an object is always the last entry
     * in a path, so we can just scan the path from the back to
     * find the last path entry
     */
    __get_object_key(ret);

    return ret;
}

Object* CreateObjectFromHandle(HANDLE handle)
{
    return __open_object(handle);
}

Object* OpenObjectFrom(Object* relative, const char* path, u32 hndl_flags, enum HNDL_MODE mode)
{
    Object* ret;
    HANDLE handle;

    /* Open the objects handle */
    if (relative)
        handle = open_handle_from(relative->handle, path, HNDL_TYPE_OBJECT, hndl_flags, mode);
    else
        handle = open_handle(path, HNDL_TYPE_OBJECT, hndl_flags, mode);

    /* Create the object */
    ret = __open_object(handle);

    if (!ret)
        close_handle(handle);

    return ret;
}

Object* OpenObjectFromIdx(Object* relative, u32 idx, u32 hndl_flags, enum HNDL_MODE mode)
{
    HANDLE handle;
    Object* ret;

    handle = sys_open_idx(relative->handle, idx, handle_flags(hndl_flags, HNDL_TYPE_OBJECT, HNDL_INVAL));

    ret = __open_object(handle);

    if (!ret)
        close_handle(handle);

    return ret;
}

Object* OpenConnectedObjectFromIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode)
{
    HANDLE handle;
    Object* ret;

    handle = sys_open_connected_idx(rel->handle, idx, handle_flags(hndl_flags, HNDL_TYPE_OBJECT, HNDL_INVAL));

    ret = __open_object(handle);

    if (!ret)
        close_handle(handle);

    return ret;
}

error_t CloseObject(Object* obj)
{
    if (!obj)
        return -EINVAL;

    if (close_handle(obj->handle))
        return -EINVAL;

    memset(obj->key, 0, sizeof(obj->key));

    obj->type = OT_NONE;
    obj->handle = HNDL_INVAL;

    /* Free this object */
    free(obj);

    return 0;
}

bool ObjectIsValid(Object* obj)
{
    return (obj && obj->type && handle_verify(obj->handle) == 0);
}

error_t ObjectSetType(Object* obj, enum OSS_OBJECT_TYPE newtype)
{
    if (!obj)
        return -EINVAL;

    return sys_set_object_type(obj->handle, newtype);
}

ssize_t ObjectRead(Object* obj, u64 offset, void* buffer, size_t size)
{
    if (!ObjectIsValid(obj))
        return -EINVAL;

    return handle_read(obj->handle, offset, buffer, size);
}

ssize_t ObjectWrite(Object* obj, u64 offset, void* buffer, size_t size)
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
