#include "fs/dir.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/syscall.h"
#include "mem/kmem.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <libk/string.h>

/*
 * When writing to a handle (filediscriptor in unix terms) we have to
 * obey the privileges that the handle was opened with
 */
ssize_t sys_write(HANDLE handle, u64 offset, void __user* buffer, size_t length)
{
    proc_t* current_proc;
    khandle_t* khandle;

    if (!buffer)
        return -EINVAL;

    current_proc = get_current_proc();

    if (!current_proc || kmem_validate_ptr(current_proc, (uintptr_t)buffer, length))
        return -EINVAL;

    khandle = find_khandle(&current_proc->m_handle_map, handle);

    if ((khandle->flags & HF_WRITEACCESS) != HF_WRITEACCESS)
        return -EPERM;

    return oss_object_write(khandle->object, offset, buffer, length);
}

/*!
 * @brief: sys_read returns the amount of bytes read
 *
 * NOTE: Device reads and file reads exibit the same behaviour regarding their offset (Meaning they are
 * both treaded like data-streams by the kernel). It's up to the underlying libc to abstract devices and
 * files further.
 */
ssize_t sys_read(HANDLE handle, u64 offset, void* buffer, size_t size)
{
    proc_t* current_proc;
    khandle_t* khandle;

    if (!buffer)
        return NULL;

    current_proc = get_current_proc();

    if (!current_proc || kmem_validate_ptr(current_proc, (uintptr_t)buffer, size))
        return NULL;

    khandle = find_khandle(&current_proc->m_handle_map, handle);

    if ((khandle->flags & HF_READACCESS) != HF_READACCESS)
        return -EPERM;

    return oss_object_read(khandle->object, offset, buffer, size);
}

size_t sys_seek(HANDLE handle, u64 c_offset, u64 new_offset, u32 type)
{
    u64 ret = c_offset;
    khandle_t* khndl;
    file_t* file;
    proc_t* curr_prc;

    curr_prc = get_current_proc();

    if (!curr_prc)
        return EINVAL;

    khndl = find_khandle(&curr_prc->m_handle_map, handle);

    /* Can only seek on objects */
    if (!khndl)
        return EINVAL;

    switch (type) {
    case 0:
        ret = new_offset;
        break;
    case 1:
        ret += new_offset;
        break;
    case 2: {
        switch (khndl->object->type) {
        case OT_FILE:
            file = oss_object_unwrap(khndl->object, OT_FILE);

            /* This has to be non-null at this point lol */
            ASSERT(file);

            /* Set the handles offset */
            ret = file->m_total_size + new_offset;
            break;
        default:
            break;
        }
        break;
    }
    }

    return ret;
}
