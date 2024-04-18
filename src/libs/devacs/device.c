#include "device.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"
#include "lightos/syscall.h"

DEV_HANDLE open_device(const char* path, DWORD flags)
{
  return open_handle(path, HNDL_TYPE_DEVICE, flags, NULL);
}

BOOL close_device(DEV_HANDLE handle)
{
  return close_handle(handle);
}

BOOL handle_is_device(HANDLE handle)
{
  BOOL res;
  HANDLE_TYPE type;

  /* Check the handle type */
  res = get_handle_type(handle, &type);

  if (!res || type != HNDL_TYPE_DEVICE)
    return FALSE;

  return TRUE;
}

size_t device_read(DEV_HANDLE handle, VOID* buf, size_t bsize, QWORD offset)
{
  BOOL res;
  QWORD end_offset;

  /* Check if we're actually reading from a device */
  res = handle_is_device(handle);

  if (!res)
    return NULL;

  /* Set the offset to read at */
  res = handle_set_offset(handle, offset);

  if (!res)
    return NULL;

  /* Do the actual read on the handle */
  res = handle_read(handle, bsize, buf);

  if (!res)
    return NULL;

  end_offset = NULL;

  /* Get the offset at the end of the read */
  res = handle_get_offset(handle, &end_offset);

  if (!res || offset > end_offset)
    return NULL;

  return (end_offset - offset);
}

size_t device_write(DEV_HANDLE handle, VOID* buf, size_t bsize, QWORD offset) 
{
  BOOL res;
  QWORD end_offset;

  /* Check if we're actually writing to a device */
  res = handle_is_device(handle);

  if (!res)
    return NULL;

  /* Set the offset to write at */
  res = handle_set_offset(handle, offset);

  if (!res)
    return NULL;

  /* Do the actual write on the handle */
  res = handle_write(handle, bsize, buf);

  if (!res)
    return NULL;

  end_offset = NULL;

  /* Get the offset at the end of the write */
  res = handle_get_offset(handle, &end_offset);

  if (!res || offset > end_offset)
    return NULL;

  return (end_offset - offset);
}

BOOL device_query_info(DEV_HANDLE handle, DEVINFO* binfo)
{
  return (syscall_2(SYSID_GET_DEVINFO, handle, (uintptr_t)binfo) == SYS_OK);
}
