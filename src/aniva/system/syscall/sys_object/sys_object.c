#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "logging/log.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <libk/math/math.h>

enum OSS_OBJECT_TYPE sys_get_object_type(HANDLE handle)
{
    khandle_t* khandle;
    proc_t* c_proc;

    c_proc = get_current_proc();
    khandle = find_khandle(&c_proc->m_handle_map, handle);

    /* Check if we have a valid handle */
    if (!khandle || khandle->type != HNDL_TYPE_OBJECT)
        return OT_INVALID;

    /* Export the type */
    return khandle->object->type;
}

enum OSS_OBJECT_TYPE sys_set_object_type(HANDLE handle, enum OSS_OBJECT_TYPE ptype)
{
    khandle_t* khandle;
    oss_object_t* result;
    proc_t* c_proc;

    if (!ptype)
        return OT_INVALID;

    c_proc = get_current_proc();
    khandle = find_khandle(&c_proc->m_handle_map, handle);

    /* Check if we have a valid handle */
    if (!khandle || khandle->type != HNDL_TYPE_OBJECT)
        return OT_INVALID;

    /* Tries to set the type */
    if (oss_object_settype(khandle->object, ptype, &result))
        return OT_COULDNT_SET;

    /* Set the new object in the handle */
    khandle->object = result;

    /* Return the resulting object type */
    return khandle->object->type;
}

error_t sys_get_object_key(HANDLE handle, char* key_buff, size_t key_buff_len)
{
    proc_t* c_proc;
    khandle_t* khandle;

    c_proc = get_current_proc();

    /* Find this handle */
    khandle = find_khandle(&c_proc->m_handle_map, handle);

    if (!khandle)
        return -EBADHANDLE;

    if (khandle->type != HNDL_TYPE_OBJECT)
        return -EBADHANDLE;

    if ((khandle->flags & HF_R) != HF_R)
        return -EPERM;

    sfmt_sz(key_buff, key_buff_len, "%s", khandle->object->key);

    return 0;
}

error_t sys_set_object_key(HANDLE handle, char* key_buff, size_t key_buff_len)
{
    proc_t* c_proc;
    khandle_t* khandle;
    char __key_buff[MIN(OSS_OBJECT_MAX_KEY_LEN, key_buff_len)];

    /* Check this guy */
    if (key_buff_len > OSS_OBJECT_MAX_KEY_LEN)
        return -E2BIG;

    c_proc = get_current_proc();

    /* Find this handle */
    khandle = find_khandle(&c_proc->m_handle_map, handle);

    if (!khandle)
        return -EBADHANDLE;

    if (khandle->type != HNDL_TYPE_OBJECT)
        return -EBADHANDLE;

    /* Check for write perms */
    if ((khandle->flags & HF_W) != HF_W)
        return -EPERM;

    /* Copy the string into the temporary buffer */
    strncpy(__key_buff, key_buff, MIN(OSS_OBJECT_MAX_KEY_LEN, key_buff_len));

    /* Call the rename function */
    return oss_object_rename(khandle->object, key_buff);
}

static error_t __get_object_and_parent(proc_t* c_proc, HANDLE object, oss_object_t** pobject, HANDLE parent, oss_object_t** pparent)
{
    khandle_t* object_khandle;
    khandle_t* parent_khandle;

    object_khandle = find_khandle(&c_proc->m_handle_map, object);

    if (!object_khandle || object_khandle->type != HNDL_TYPE_OBJECT)
        return -EINVAL;

    parent_khandle = find_khandle(&c_proc->m_handle_map, parent);

    if (!parent_khandle || object_khandle->type != HNDL_TYPE_OBJECT)
        return -EINVAL;

    *pobject = object_khandle->object;
    *pparent = parent_khandle->object;

    return 0;
}

/*!
 * @brief: Checks if @proc has permission to connect @object to @parent
 *
 * TODO: Implement object permissions
 */
static bool __can_proc_connect_objects(proc_t* proc, oss_object_t* object, oss_object_t* parent)
{
    return true;
}

error_t sys_connect_object(HANDLE object_hndl, HANDLE new_parent_hndl)
{
    error_t error;
    proc_t* c_proc;
    oss_object_t *object, *parent;

    c_proc = get_current_proc();

    /* Get the two objects */
    error = __get_object_and_parent(c_proc, object_hndl, &object, new_parent_hndl, &parent);

    if (error)
        return error;

    /* Check if we can actually do the connect */
    if (!__can_proc_connect_objects(c_proc, object, parent))
        return -EPERM;

    /* Try to connect them */
    return oss_object_connect(parent, object);
}

/*!
 * @brief: Checks if @proc has permission to disconnect @object from @parent
 *
 * TODO: Implement object permissions
 */
static bool __can_proc_disconnect_objects(proc_t* proc, oss_object_t* object, oss_object_t* parent)
{
    return true;
}

error_t sys_disconnect_object(HANDLE object_hndl, HANDLE parent_hndl)
{
    error_t error;
    proc_t* c_proc;
    oss_object_t *object, *parent;

    c_proc = get_current_proc();

    /* Get the two objects */
    error = __get_object_and_parent(c_proc, object_hndl, &object, parent_hndl, &parent);

    if (error)
        return error;

    /* Check if we can actually do the disconnect */
    if (!__can_proc_disconnect_objects(c_proc, object, parent))
        return -EPERM;

    /* Try to disconnect them */
    return oss_object_disconnect(parent, object);
}
