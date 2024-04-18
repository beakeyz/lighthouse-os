#ifndef __LIGHTOS_DEVACS_DEVICE__
#define __LIGHTOS_DEVACS_DEVICE__

#include <lightos/handle.h>
#include <devacs/shared.h>

/*
 * TODO: Device error codes
 */

/* Our own quick typedef for a handle to a device */
typedef HANDLE DEV_HANDLE;

/*!
 * @brief: Open a device object
 * @path: Path to the device, must start with Dev/
 * @flags: Any flags to pass to the device
 *
 * @returns: A handle to the device if successful. Otherwise an invalid handle
 */
DEV_HANDLE open_device(
  __IN__ const char*    path,
  __IN__ DWORD          flags
);

/*!
 * @brief: Close a device
 * @handle: The handle recieved by open_device
 *
 * @returns: true if successful, false otherwise
 */
BOOL close_device(
  __IN__ DEV_HANDLE handle
);

/*!
 * @brief: Check if a handle is for a device
 * @handle: The handle to check
 *
 * @returns: True if the handle is for a device
 */
BOOL handle_is_device(
  __IN__ HANDLE handle
);

/*!
 * @brief: Read from a device
 * @handle: A handle of the device we want to read to
 * @buf: A buffer where data is supposed to go
 * @bsize: The size we want to read
 * @offset: Offset to read from
 *
 * @returns: The amount of bytes read
 */
size_t device_read(
  __IN__ DEV_HANDLE handle,
  __IN__ VOID*      buf,
  __IN__ size_t     bsize,
  __IN__ QWORD      offset
);

/*!
 * @brief: Write to a device
 * @handle: A handle of the device we want to write from
 * @buf: A buffer where data is supposed to go
 * @bsize: The size we want to write
 * @offset: Offset to write to
 *
 * @returns: The amount of bytes written
 */
size_t device_write(
  __IN__ DEV_HANDLE handle,
  __IN__ VOID*      buf,
  __IN__ size_t     bsize,
  __IN__ QWORD      offset
);

/*!
 * @brief: Query a device for a devinfo block
 * @handle: A handle to the device
 * @binfo: A buffer where the devinfo will be placed
 *
 * @returns: true if successful, false otherwise
 */
BOOL device_query_info(
  __IN__    DEV_HANDLE handle,
  __OUT__   DEVINFO   *binfo
);

#endif // !__LIGHTOS_DEVACS_DEVICE__
