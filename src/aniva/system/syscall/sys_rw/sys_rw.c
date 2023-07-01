
#include "sys_rw.h"
#include "LibSys/syscall.h"
#include "fs/file.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include "sched/scheduler.h"


uint64_t sys_write(handle_t handle, uint8_t __user* buffer, size_t length)
{
  int result;
  proc_t* current_proc;
  khandle_t* khandle;

  if (!buffer)
    return SYS_INV;

  current_proc = get_current_proc();

  khandle = &current_proc->m_handle_map.handles[handle];

  switch (khandle->type) {
    case KHNDL_TYPE_FILE:
      {
        size_t write_len = length;
        file_t* file = khandle->reference.file;

        if (!file || !file->m_ops || !file->m_ops->f_write)
          return SYS_INV;

        result = file->m_ops->f_write(file, buffer, &write_len, khandle->offset);

        if (result < 0)
          return SYS_KERR;

        khandle->offset += write_len;

        break;
      }
    default:
      return SYS_INV;
  }

  return SYS_OK;
}
