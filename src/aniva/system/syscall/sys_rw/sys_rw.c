
#include "sys_rw.h"
#include "LibSys/handle_def.h"
#include "LibSys/syscall.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

/*
 * When writing to a handle (filediscriptor in unix terms) we have to 
 * obey the privileges that the handle was opened with
 */
uint64_t sys_write(handle_t handle, uint8_t __user* buffer, size_t length)
{
  int result;
  proc_t* current_proc;
  khandle_t* khandle;

  if (!buffer)
    return SYS_INV;

  current_proc = get_current_proc();

  if (!current_proc || IsError(kmem_validate_ptr(current_proc, (uintptr_t)buffer, length)))
    return SYS_INV;

  khandle = find_khandle(&current_proc->m_handle_map, handle);

  if ((khandle->flags & HNDL_FLAG_WRITEACCESS) != HNDL_FLAG_WRITEACCESS)
    return SYS_NOPERM;
  
  switch (khandle->type) {
    case KHNDL_TYPE_FILE:
      {
        size_t write_len = length;
        file_t* file = khandle->reference.file;

        if (!file || !file->m_ops || !file->m_ops->f_write)
          return SYS_INV;

        /* Don't read RO files */
        if (file->m_flags & FILE_READONLY)
          return SYS_INV;

        result = file->m_ops->f_write(file, buffer, &write_len, khandle->offset);

        if (result < 0)
          return SYS_KERR;

        khandle->offset += write_len;

        break;
      }
    case KHNDL_TYPE_DRIVER:
      {
        int result;
        size_t buffer_size = length;
        aniva_driver_t* driver = khandle->reference.driver;

        result = drv_write(driver, buffer, &buffer_size, khandle->offset);

        if (result == DRV_STAT_OK)
          break;

        kernel_panic("Failed to write to driver");
        return SYS_KERR;
      }
    case KHNDL_TYPE_PROC:
      {

        break;
      }
    default:
      return SYS_INV;
  }

  return SYS_OK;
}

/*
 * sys_read returns the amount of bytes read
 */
uint64_t sys_read(handle_t handle, uint8_t __user* buffer, size_t length)
{
  int result;
  size_t read_len;
  proc_t* current_proc;
  khandle_t* khandle;

  if (!buffer)
    return NULL;

  current_proc = get_current_proc();

  if (!current_proc || 
      IsError(kmem_validate_ptr(current_proc, (uintptr_t)buffer, length)))
    return NULL;

  khandle = find_khandle(&current_proc->m_handle_map, handle);

  if ((khandle->flags & HNDL_FLAG_READACCESS) != HNDL_FLAG_READACCESS)
    return SYS_NOPERM;

  switch (khandle->type) {
    case KHNDL_TYPE_FILE:
      {
        read_len = length;
        file_t* file = khandle->reference.file;

        if (!file || !file->m_ops || !file->m_ops->f_write)
          return NULL;

        result = file->m_ops->f_read(file, buffer, &read_len, khandle->offset);

        if (result < 0)
          return NULL;

        khandle->offset += read_len;

        break;
      }
    case KHNDL_TYPE_DRIVER:
      {
        int result;
        aniva_driver_t* driver = khandle->reference.driver;

        result = drv_read(driver, buffer, &read_len, khandle->offset);

        if (result != DRV_STAT_OK)
          return NULL;

        khandle->offset += read_len;

        break;
      }
    case KHNDL_TYPE_PROC:
      {
        break;
      }
    default:
      return NULL;
  }

  return read_len;
}
