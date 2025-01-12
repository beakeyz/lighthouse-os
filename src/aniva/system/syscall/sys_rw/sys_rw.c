#include "fs/dir.h"
#include "fs/file.h"
#include "lightos/api/filesystem.h"
#include "lightos/api/handle.h"
#include "lightos/syscall.h"
#include "mem/kmem.h"
#include "oss/obj.h"
#include "proc/handle.h"
#include "proc/hdrv/driver.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <libk/string.h>
#include <proc/env.h>

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
    proc_t* curr_prc;

    curr_prc = get_current_proc();

    if (!curr_prc)
        return EINVAL;

    khndl = find_khandle(&curr_prc->m_handle_map, handle);

    if (!khndl)
        return EINVAL;

    switch (type) {
    case 0:
        khndl->offset = offset;
        break;
    case 1:
        khndl->offset += offset;
        break;
    case 2: {
        switch (khndl->type) {
        case HNDL_TYPE_FILE:
            khndl->offset = khndl->reference.file->m_total_size + offset;
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
uint64_t sys_dir_read(handle_t handle, uint32_t idx, lightos_direntry_t __user* b_dirent, size_t blen)
{
    proc_t* c_proc;
    khandle_t* khandle;
    khandle_driver_t* khandle_driver;

    /* No driver to read directories, fuckkk */
    if (khandle_driver_find(HNDL_TYPE_DIR, &khandle_driver))
        return EINVAL;

    c_proc = get_current_proc();

    /* Check that our direntry is valid */
    if (kmem_validate_ptr(c_proc, (vaddr_t)b_dirent, 1))
        return EINVAL;

    /* Don't want to go poking in kernel memory */
    if ((vaddr_t)b_dirent >= KERNEL_MAP_BASE)
        return EINVAL;

    /* Find the right underlying dir object */
    khandle = find_khandle(&c_proc->m_handle_map, handle);

    if (!khandle || !khandle->reference.kobj)
        return EINVAL;

    if (khandle->type != HNDL_TYPE_DIR)
        return EINVAL;

    /* Set the right offset */
    khandle->offset = idx;

    /* bsize is unused by this syscall */
    return khandle_driver_read(khandle_driver, khandle, b_dirent, NULL);
}
