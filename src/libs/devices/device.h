#ifndef __LIGHTOS_DEVICES_DEVICE__
#define __LIGHTOS_DEVICES_DEVICE__

#include "lightos/system.h"
#include <devices/shared.h>
#include <lightos/handle.h>

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
    __IN__ const char* path,
    __IN__ DWORD flags);

/*!
 * @brief: Close a device
 * @handle: The handle recieved by open_device
 *
 * @returns: true if successful, false otherwise
 */
BOOL close_device(
    __IN__ DEV_HANDLE handle);

/*!
 * @brief: Check if a handle is for a device
 * @handle: The handle to check
 *
 * @returns: True if the handle is for a device
 */
BOOL handle_is_device(
    __IN__ HANDLE handle);

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
    __IN__ VOID* buf,
    __IN__ size_t bsize,
    __IN__ QWORD offset);

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
    __IN__ VOID* buf,
    __IN__ size_t bsize,
    __IN__ QWORD offset);

/*!
 * @brief: Enable a device
 * @handle: Handle to the device to enable
 *
 * @returns: True if the device was successfully enabled, or if it
 * already was enabled. False otherwise
 */
BOOL device_enable(
    __IN__ DEV_HANDLE handle);

/*!
 * @brief: Disable a device
 * @handle: Handle to the device to disable
 *
 * @returns: True if the device was successfully disabled, or if it
 * already was disabled. False otherwise
 */
BOOL device_disable(
    __IN__ DEV_HANDLE handle);

BOOL device_send_ctl(
    __IN__ DEV_HANDLE handle,
    __IN__ enum DEVICE_CTLC code);

/*!
 * @brief: Send a control message to a device
 * @handle: The handle to the device to message
 * @dcc: The device control code
 * @buf: The buffer to send
 * @bsize: The size of the buffer
 */
BOOL device_send_ctl_ex(
    __IN__ DEV_HANDLE handle,
    __IN__ enum DEVICE_CTLC code,
    __IN__ __OPTIONAL__ uintptr_t offset,
    __IN__ __OPTIONAL__ VOID* buf,
    __IN__ __OPTIONAL__ size_t bsize);

/*!
 * @brief: Query a device for a devinfo block
 * @handle: A handle to the device
 * @binfo: A buffer where the devinfo will be placed
 *
 * @returns: true if successful, false otherwise
 */
BOOL device_query_info(
    __IN__ DEV_HANDLE handle,
    __OUT__ DEVINFO* binfo);

#endif // !__LIGHTOS_DEVICES_DEVICE__
