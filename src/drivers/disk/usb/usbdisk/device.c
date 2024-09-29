#include "dev/device.h"
#include "dev/disk/generic.h"
#include "dev/usb/usb.h"
#include "devices/shared.h"
#include "libk/flow/error.h"
#include "mem/heap.h"

struct usbdisk_dev {
    usb_device_t* udev;
    disk_dev_t* diskdev;
};

/*!
 * @brief: Read a block from the usbstick
 */
static int usbdisk_bread(struct device* device, struct driver* driver, u64 offset, void* buffer, size_t bsize)
{
    return 0;
}

/*!
 * @brief: Write a block to the usbstick
 */
static int usbdisk_bwrite(struct device* device, struct driver* driver, u64 offset, void* buffer, size_t bsize)
{
    return 0;
}

static disk_dev_ops_t __usbdisk_ops = {
    .f_bread = usbdisk_bread,
    .f_bwrite = usbdisk_bwrite,
};

/*!
 * @brief: Creates a new usbdisk device object
 *
 * Should also init the backing hardware to a state where we can send block transactions
 * to the device.
 */
int usbdisk_create(driver_t* driver, usb_device_t* dev, usb_interface_buffer_t* intrf)
{
    struct usbdisk_dev* uddev;

    uddev = kmalloc(sizeof(*uddev));

    if (!uddev)
        return -KERR_NOMEM;

    uddev->udev = dev;
    uddev->diskdev = create_generic_disk(driver, "usbdisk", uddev, DEVICE_CTYPE_USB, &__usbdisk_ops);

    /* Register the generic disk device (Because idk) */
    register_gdisk_dev(uddev->diskdev);

    /* TODO: Register the disk device to oss (Should gdisk do this?) */
    return 0;
}

int usbdisk_destroy(struct usbdisk_dev* dev)
{
    destroy_generic_disk(dev->diskdev);

    kfree(dev);
    return 0;
}
