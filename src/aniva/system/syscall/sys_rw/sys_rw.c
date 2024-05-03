
#include "sys_rw.h"
#include "dev/device.h"
#include "fs/dir.h"
#include "lightos/handle_def.h"
#include "lightos/proc/var_types.h"
#include "lightos/syscall.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "fs/file.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "oss/obj.h"
#include "proc/handle.h"
#include "proc/proc.h"
#include <libk/string.h>
#include "sched/scheduler.h"
#include "system/sysvar/var.h"
#include <proc/env.h>

/*
 * When writing to a handle (filediscriptor in unix terms) we have to 
 * obey the privileges that the handle was opened with
 */
uint64_t sys_write(handle_t handle, uint8_t __user* buffer, size_t length)
{
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
    case HNDL_TYPE_FILE:
      {
        size_t write_len = length;
        file_t* file = khandle->reference.file;

        if (!file || !file->m_ops || !file->m_ops->f_write)
          return SYS_INV;

        /* Don't read RO files */
        if (file->m_flags & FILE_READONLY)
          return SYS_INV;

        write_len = file_write(file, buffer, write_len, khandle->offset);

        if (!write_len)
          return SYS_KERR;

        khandle->offset += write_len;
        break;
      }
    case HNDL_TYPE_DRIVER:
      {
        int result;
        size_t buffer_size = length;
        drv_manifest_t* driver = khandle->reference.driver;

        result = drv_write(driver, buffer, &buffer_size, khandle->offset);

        if (result == DRV_STAT_OK)
          break;

        kernel_panic("Failed to write to driver");
        return SYS_KERR;
      }
    case HNDL_TYPE_PROC:
      {

        break;
      }

    case HNDL_TYPE_PROFILE:
      {
        kernel_panic("TODO: implement writes to process profiles!");
        break;
      }
    case HNDL_TYPE_SYSVAR:
      {
        /* TODO: */
        kernel_panic("TODO: implement pvar writes with the appropriate permission checks");
        break;
      }
    default:
      return SYS_INV;
  }

  return SYS_OK;
}

/*!
 * @brief: sys_read returns the amount of bytes read
 *
 * NOTE: Device reads and file reads exibit the same behaviour regarding their offset (Meaning they are
 * both treaded like data-streams by the kernel). It's up to the underlying libc to abstract devices and
 * files further.
 */
uint64_t sys_read(handle_t handle, uint8_t __user* buffer, size_t length)
{
  size_t read_len;
  proc_t* current_proc;
  khandle_t* khandle;

  if (!buffer)
    return NULL;

  current_proc = get_current_proc();

  if (!current_proc || 
      IsError(kmem_validate_ptr(current_proc, (uintptr_t)buffer, length)))
    return NULL;

  read_len = 0;
  khandle = find_khandle(&current_proc->m_handle_map, handle);

  if ((khandle->flags & HNDL_FLAG_READACCESS) != HNDL_FLAG_READACCESS)
    return SYS_NOPERM;

  switch (khandle->type) {
    case HNDL_TYPE_FILE:
      {
        file_t* file = khandle->reference.file;

        if (!file || !file->m_ops || !file->m_ops->f_read)
          return NULL;

        read_len = file_read(file, buffer, length, khandle->offset);

        if (!read_len)
          return -SYS_KERR;

        khandle->offset += read_len;
        break;
      }
    case HNDL_TYPE_DEVICE:
      {
        device_t* device = khandle->reference.device;

        if (!device || !oss_obj_can_proc_read(device->obj, current_proc))
          return NULL;

        read_len = 0;

        if (device_read(device, buffer, khandle->offset, length) == KERR_NONE) {
          khandle->offset += length;
          read_len = length;
        }
        break;
      }
    case HNDL_TYPE_DRIVER:
      {
        int result;
        drv_manifest_t* driver = khandle->reference.driver;

        result = drv_read(driver, buffer, &length, khandle->offset);

        if (result != DRV_STAT_OK)
          return NULL;

        read_len = length;

        khandle->offset += read_len;
        break;
      }
    case HNDL_TYPE_PROC:
      {
        break;
      }
    case HNDL_TYPE_PROFILE:
      {
        kernel_panic("TODO: implement reads to process profiles!");
        break;
      }
    case HNDL_TYPE_SYSVAR:
      {
        size_t data_size = NULL;
        size_t minimal_buffersize = 1;
        sysvar_t* var = khandle->reference.pvar;
        penv_t* c_env = current_proc->m_env;

        /* Check if the current environment actualy has permission to read from this var */
        if (var->obj->read_priv_lvl > c_env->priv_level)
          return NULL;

        switch (var->type) {
          case SYSVAR_TYPE_STRING:
            /* We need at least the null-byte */
            minimal_buffersize = 1;
            data_size = strlen(var->str_value) + 1;
            break;
          case SYSVAR_TYPE_QWORD:
            data_size = minimal_buffersize = sizeof(uint64_t);
            break;
          case SYSVAR_TYPE_DWORD:
            data_size = minimal_buffersize = sizeof(uint32_t);
            break;
          case SYSVAR_TYPE_WORD:
            data_size = minimal_buffersize = sizeof(uint16_t);
            break;
          case SYSVAR_TYPE_BYTE:
            data_size = minimal_buffersize = sizeof(uint8_t);
            break;
          default:
            break;
        }

        /* Check if we have enough space to store the value */
        if (!data_size || length < minimal_buffersize)
          return NULL;

        /* Make sure we are not going to copy too much into the user buffer */
        if (data_size > length)
          data_size = length;

        /*
         * TODO: move these raw memory opperations
         * to a controlled environment for safe copies and 
         * 'memsets' into userspace
         */
        
        memcpy(buffer, var->value, data_size);

        if (var->type == SYSVAR_TYPE_STRING)
          buffer[length-1] = '\0';

        read_len = data_size;
        break;
      }
    default:
      return NULL;
  }

  return read_len;
}

uint64_t sys_seek(handle_t handle, uintptr_t offset, uint32_t type)
{
  khandle_t* khndl;
  proc_t* curr_prc;

  curr_prc = get_current_proc();

  if (!curr_prc)
    return SYS_ERR;

  khndl = find_khandle(&curr_prc->m_handle_map, handle);

  if (!khndl)
    return SYS_INV;

  switch (type) {
    case 0:
      khndl->offset = offset;
      break;
    case 1:
      khndl->offset += offset;
      break;
    case 2:
      {
        switch (khndl->type) {
          case HNDL_TYPE_FILE:
            khndl->offset = khndl->reference.file->m_total_size + offset;
            break;
        }
        break;
      }
  }

  return khndl->offset;
}

/*!
 * @brief: Read from a directory at index @idx
 */
uint64_t sys_dir_read(handle_t handle, uint32_t idx, char __user* namebuffer, size_t blen)
{
  dir_t* target_dir;
  direntry_t target_entry;
  const char* target_name;
  proc_t* c_proc;
  khandle_t* khandle;

  c_proc = get_current_proc();

  if (IsError(kmem_validate_ptr(c_proc, (vaddr_t)namebuffer, 1)))
    return SYS_INV;

  /* Don't want to go poking in kernel memory */
  if ((vaddr_t)namebuffer >= KERNEL_MAP_BASE)
    return SYS_INV;

  khandle = find_khandle(&c_proc->m_handle_map, handle);

  if (!khandle || !khandle->reference.kobj)
    return SYS_INV;

  if (khandle->type != HNDL_TYPE_DIR)
    return SYS_INV;

  target_dir = khandle->reference.dir;

  if (idx >= target_dir->child_capacity)
    return SYS_INV;

  /* Read into the entry */
  if (!KERR_OK(dir_read(target_dir, idx, &target_entry)))
    return SYS_INV;

  /* Clear the buffer */
  memset(namebuffer, 0, blen);

  switch (target_entry.type) {
    case DIRENT_TYPE_DIR:
      target_name = target_entry.dir->name;
      break;
    case DIRENT_TYPE_FILE:
      target_name = target_entry.file->m_obj->name;
      break;
    case DIRENT_TYPE_OBJ:
      target_name = target_entry.obj->name;
      break;
    default:
      target_name = nullptr;
  }

  if (!target_name)
    goto close_and_fail;

  /* Truncate the length */
  if (strlen(target_name) < blen)
    blen = strlen(target_name);

  /* Copy */
  strncpy(namebuffer, target_name, blen);

  close_direntry(&target_entry);
  return SYS_OK;

close_and_fail:
  close_direntry(&target_entry);
  return SYS_INV;
}
