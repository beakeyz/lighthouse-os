#include "fs/dir.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "lightos/api/objects.h"
#include "lightos/syscall.h"
#include "mem/kmem.h"
#include "oss/object.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <libk/string.h>

/*
 * When writing to a handle (filediscriptor in unix terms) we have to
 * obey the privileges that the handle was opened with
 */
error_t sys_write(HANDLE handle, u64 offset, void __user* buffer, size_t length)
{
    proc_t* current_proc;
    khandle_t* khandle;
    khandle_driver_t* khandle_driver;

    if (!buffer)
        return EINVAL;

    current_proc = get_current_proc();

    if (!current_proc || kmem_validate_ptr(current_proc, (uintptr_t)buffer, length))
        return EINVAL;

    khandle = find_khandle(&current_proc->m_handle_map, handle);

    if ((khandle->flags & HNDL_FLAG_WRITEACCESS) != HNDL_FLAG_WRITEACCESS)
        return EPERM;

    /* If we can't find a driver, the system does not support writing to this type of handle
     * in it's current state... */
    if (khandle_driver_find(khandle->type, &khandle_driver))
        return EINVAL;

    if (khandle_driver_write(khandle_driver, khandle, offset, buffer, length))
        return 0;

    /* TODO: Return actual written length */
    return 0;
}

/*!
 * @brief: sys_read returns the amount of bytes read
 *
 * NOTE: Device reads and file reads exibit the same behaviour regarding their offset (Meaning they are
 * both treaded like data-streams by the kernel). It's up to the underlying libc to abstract devices and
 * files further.
 */
error_t sys_read(HANDLE handle, u64 offset, void* buffer, size_t size, size_t* pread_size)
{
    error_t error;
    proc_t* current_proc;
    khandle_t* khandle;
    khandle_driver_t* khandle_driver;

    if (!buffer)
        return NULL;

    current_proc = get_current_proc();

    if (!current_proc || kmem_validate_ptr(current_proc, (uintptr_t)buffer, size))
        return NULL;

    khandle = find_khandle(&current_proc->m_handle_map, handle);

    if ((khandle->flags & HNDL_FLAG_READACCESS) != HNDL_FLAG_READACCESS)
        return EPERM;

    /* If we can't find a driver, the system does not support reading this type of handle
     * in it's current state... */
    if (khandle_driver_find(khandle->type, &khandle_driver))
        return EINVAL;

    error = khandle_driver_read(khandle_driver, khandle, offset, buffer, size);

    /* FIXME: Do we need to check this address? */
    if (pread_size)
        *pread_size = size;

    return error;
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
    if (!khndl || khndl->type != HNDL_TYPE_OBJECT)
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

error_t sys_dir_create(const char* path, i32 mode)
{
    // kernel_panic("TODO: Implement syscall (sys_dir_create)");
    return 0;
}
