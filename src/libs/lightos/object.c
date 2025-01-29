#include "object.h"
#include "lightos/api/handle.h"
#include "lightos/handle.h"
#include "lightos/syscall.h"
#include "stdlib.h"
#include <lightos/api/objects.h>
#include <stdio.h>
#include <string.h>

/*
 * Reference to the working object holder. Every process should be launched
 * wich a CWO object attached, which is defined by the api/objects.h file.
 */
static Object* __wo_holder = NULL;

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

    ret->handle = open_handle(key, HF_RW, type, HNDL_MODE_CREATE_NEW);

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

Object* OpenObject(const char* path, u32 hndl_flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode)
{
    return OpenObjectFrom(nullptr, path, hndl_flags, type, mode);
}

/*!
 * @brief: Creates a new userspace object based on @handle
 *
 * Assumes @handle is valid
 */
static Object* __open_object(HANDLE handle)
{
    Object* ret;
    enum OSS_OBJECT_TYPE type;

    /* First, try to get an object type from this handle */
    type = __get_object_type(handle);

    /* Check if that is actually a valid type */
    if (!oss_object_valid_type(type))
        return nullptr;

    /* Yes, now we can allocate a new object */
    ret = malloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    /* Clear the object buffer */
    memset(ret, 0, sizeof(*ret));

    /* Set the objects handle */
    ret->handle = handle;
    /* Emplace our previously gotten object type */
    ret->type = type;

    /* Get the object key from the path */
    __get_object_key(ret);

    return ret;
}

Object* OpenSelfObject(u32 flags)
{
    HANDLE self;

    /* Try to open ourselves */
    self = sys_open_proc_obj(NULL, handle_flags(flags, HNDL_INVAL));

    /* Try to create an object from it */
    return GetObjectFromHandle(self);
}

Object* GetObjectFromHandle(HANDLE handle)
{
    /* Check if this handle is actually valid */
    if (IS_FATAL(handle_verify(handle)))
        return nullptr;

    return __open_object(handle);
}

Object* OpenObjectFrom(Object* rel, const char* path, u32 hndl_flags, enum OSS_OBJECT_TYPE type, enum HNDL_MODE mode)
{
    Object* ret;
    HANDLE handle;

    /* Open the objects handle */
    if (rel)
        handle = open_handle_from(rel->handle, path, hndl_flags, type, mode);
    else
        handle = open_handle(path, hndl_flags, type, mode);

    /* Check if this handle is actually valid */
    if (IS_FATAL(handle_verify(handle)))
        return nullptr;

    /* Create the object */
    ret = __open_object(handle);

    if (!ret)
        close_handle(handle);

    return ret;
}

Object* OpenObjectIdx(Object* relative, u32 idx, u32 hndl_flags, enum HNDL_MODE mode)
{
    HANDLE handle;
    Object* ret;

    handle = sys_open_idx(relative->handle, idx, handle_flags(hndl_flags, HNDL_INVAL));

    ret = __open_object(handle);

    if (!ret)
        close_handle(handle);

    return ret;
}

Object* OpenConnectedObjectByIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode)
{
    HANDLE handle;
    Object* ret;

    handle = sys_open_connected_idx(rel->handle, idx, handle_flags(hndl_flags, HNDL_INVAL));

    ret = __open_object(handle);

    if (!ret)
        close_handle(handle);

    return ret;
}

Object* OpenDownConnectedObjectByIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode)
{
    return OpenConnectedObjectByIdx(rel, idx, hndl_flags | HF_D, mode);
}

Object* OpenUpConnectedObjectByIdx(Object* rel, u32 idx, u32 hndl_flags, enum HNDL_MODE mode)
{
    return OpenConnectedObjectByIdx(rel, idx, hndl_flags | HF_U, mode);
}

Object* OpenWorkingObject(u32 flags)
{
    Object* self_obj = OpenSelfObject(HF_RW);

    /* Big bruh */
    if (!self_obj)
        return nullptr;

    /*
     * Open the working object holder on our process
     */
    Object* woh = OpenObjectFrom(self_obj, OBJECT_WO_HOLDER, HF_R, OT_ANY, NULL);

    /* Close this guy again as well */
    CloseObject(self_obj);

    /* No woh. Sad =( */
    if (!ObjectIsValid(woh))
        return nullptr;

    /* Try to open the object pointed to by WO */
    return OpenDownConnectedObjectByIdx(woh, 0, NULL, NULL);
}

error_t ChWorkingObject(Object* object)
{
    exit_noimpl("ChWorkingObject");
    return 0;
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
    if (!ObjectIsValid(obj) || !ObjectIsValid(child))
        return -EINVAL;

    return sys_connect_object(child->handle, obj->handle);
}

error_t ObjectDisconnect(Object* obj, Object* child)
{
    if (!obj || !child)
        return -EINVAL;

    return sys_disconnect_object(child->handle, obj->handle);
}

error_t ObjectGetInfo(Object* object, object_info_t* infobuff)
{
    exit_noimpl("ObjectGetInfo");
    return 0;
}

void __init_lightos_oss_objects()
{
    (void)__wo_holder;
}
