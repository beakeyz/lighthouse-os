
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

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
    proc_t* c_proc;

    if (!ptype)
        return OT_INVALID;

    c_proc = get_current_proc();
    khandle = find_khandle(&c_proc->m_handle_map, handle);

    /* Check if we have a valid handle */
    if (!khandle || khandle->type != HNDL_TYPE_OBJECT)
        return OT_INVALID;

    if (khandle->object->type != OT_NONE)
        return OT_ALREADY_SET;

    /* Tries to set the type */
    (void)oss_object_settype(khandle->object, ptype);

    /* Return the resulting object type */
    return khandle->object->type;
}
