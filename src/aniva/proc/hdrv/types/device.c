
#include "dev/device.h"
#include "devices/shared.h"
#include "libk/flow/error.h"
#include "proc/hdrv/driver.h"

static kerror_t khandle_device_driver_read(khandle_driver_t* driver, khandle_t* handle, void* buffer, size_t bsize)
{
    int error;
    device_t* dev;

    /* Grab the device object */
    dev = handle->reference.device;

    /* Read from the device */
    error = device_read(dev, buffer, handle->offset, bsize);

    if (error)
        return error;

    handle->offset += bsize;
    return 0;
}

static kerror_t khandle_device_driver_write(khandle_driver_t* driver, khandle_t* handle, void* buffer, size_t bsize)
{
    int error;
    device_t* dev;

    /* Grab the device object */
    dev = handle->reference.device;

    /* Write to the device */
    error = device_write(dev, buffer, handle->offset, bsize);

    if (error)
        return error;

    handle->offset += bsize;
    return 0;
}

static kerror_t khandle_device_driver_ctl(khandle_driver_t* driver, khandle_t* handle, enum DEVICE_CTLC ctl, u64 offset, void* buffer, size_t bsize)
{
    device_t* dev;

    /* Grab the device object */
    dev = handle->reference.device;

    /* Send a ctlcode to the device from the device */
    return device_send_ctl_ex(dev, ctl, offset, buffer, bsize);
}

static int khandle_device_driver_destroy(khandle_driver_t* driver, khandle_t* handle)
{
    device_t* dev;

    if (!handle)
        return -KERR_INVAL;

    /* Get the thing */
    dev = handle->reference.device;

    if (!dev)
        return -KERR_INVAL;

    /* Destroy the device */
    destroy_device(dev);

    return 0;
}

static khandle_driver_t device_khandle_driver = {
    .name = "devices",
    .handle_type = HNDL_TYPE_DEVICE,
    .function_flags = KHDRIVER_FUNC_ALL_IO,
    .f_open = khandle_driver_generic_oss_open,
    .f_open_relative = khandle_driver_generic_oss_open_from,
    .f_read = khandle_device_driver_read,
    .f_write = khandle_device_driver_write,
    .f_ctl = khandle_device_driver_ctl,
    .f_destroy = khandle_device_driver_destroy,
};
EXPORT_KHANDLE_DRIVER(device_khandle_driver);
