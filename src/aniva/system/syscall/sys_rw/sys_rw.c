
#include "sys_rw.h"
#include "LibSys/syscall.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "fs/file.h"
#include "libk/flow/error.h"
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

  khandle = &current_proc->m_handle_map.handles[handle];

  if ((khandle->flags & HNDL_FLAG_WRITEACCESS) != HNDL_FLAG_WRITEACCESS)
    return SYS_NOPERM;
  
  println((char*)buffer);

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
        aniva_driver_t* driver = khandle->reference.driver;

        kernel_panic("tried to write to driver");
        //driver_send_packet()
        break;
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
