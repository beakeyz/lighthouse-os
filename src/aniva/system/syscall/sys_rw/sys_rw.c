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
error_t sys_write(HANDLE handle, void __user* buffer, size_t length)
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

    if (khandle_driver_write(khandle_driver, khandle, buffer, length))
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
error_t sys_read(HANDLE handle, void* buffer, size_t size, size_t* pread_size)
{
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

    if (khandle_driver_read(khandle_driver, khandle, buffer, size))
        return 0;

    /* FIXME: Do we need to check this address? */
    if (pread_size)
        *pread_size = size;

    return 0;
}

size_t sys_seek(handle_t handle, uintptr_t offset, uint32_t type)
{
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
        khndl->offset = offset;
        break;
    case 1:
        khndl->offset += offset;
        break;
    case 2: {
        switch (khndl->object->type) {
        case OT_FILE:
            file = oss_object_unwrap(khndl->object, OT_FILE);

            /* This has to be non-null at this point lol */
            ASSERT(file);

            /* Set the handles offset */
            khndl->offset = file->m_total_size + offset;
            break;
        default:
            break;
        }
        break;
    }
    }

    return khndl->offset;
}

error_t sys_dir_create(const char* path, i32 mode)
{
    // kernel_panic("TODO: Implement syscall (sys_dir_create)");
    return 0;
}

/*!
 * @brief: Read from a directory at index @idx
 */
uint64_t sys_dir_read(handle_t handle, uint32_t idx, Object __user* b_dirent, size_t blen)
{
    dir_t* dir;
    proc_t* c_proc;
    khandle_t* dir_khandle;
    oss_object_t* object;

    c_proc = get_current_proc();

    /* Check that our direntry is valid */
    if (kmem_validate_ptr(c_proc, (vaddr_t)b_dirent, 1))
        return EINVAL;

    /* Don't want to go poking in kernel memory */
    if ((vaddr_t)b_dirent >= KERNEL_MAP_BASE)
        return EINVAL;

    /* Find the right underlying dir object */
    dir_khandle = find_khandle(&c_proc->m_handle_map, handle);

    if (!dir_khandle || dir_khandle->type != HNDL_TYPE_OBJECT)
        return EINVAL;

    /* Check if we can get a directory from this guy */
    dir = oss_object_unwrap(dir_khandle->object, OT_DIR);

    if (!dir)
        return EINVAL;

    /* Set the right offset */
    dir_khandle->offset = idx;

    /* Try to find an object at this index */
    object = dir_find_idx(dir, idx);

    if (!object)
        return ENOENT;

    b_dirent->type = object->type;
    b_dirent->handle = HNDL_INVAL;

    /* (hopefully) Safely copy the key */
    strncpy((char*)b_dirent->key, object->key, sizeof(b_dirent->key) - 1);

    /* Release the temporary reference again */
    oss_object_close(object);

    return 0;
}
