#include "dev/device.h"
#include "dev/disk/device.h"
#include "dev/disk/volume.h"
#include "dev/usb/usb.h"
#include "libk/flow/error.h"
#include "mem/heap.h"

struct usbdisk_dev {
    usb_device_t* udev;
    volume_device_t* diskdev;
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

static volume_dev_ops_t __usbdisk_ops = {
    .f_bread = usbdisk_bread,
    .f_bwrite = usbdisk_bwrite,
};

/*!
 * @brief: Gathers generic volume information
 *
 * TODO: Do the needed USB transactions for this
 */
static inline int usbdisk_init_volume_dev_info(struct usbdisk_dev* uddev, volume_info_t* pinfo)
{
    return 0;
}

/*!
 * @brief: Creates a new usbdisk device object
 *
 * Should also init the backing hardware to a state where we can send block transactions
 * to the device.
 */
int usbdisk_create(driver_t* driver, usb_device_t* dev, usb_interface_buffer_t* intrf)
{
    struct usbdisk_dev* uddev;
    volume_info_t info = { 0 };

    uddev = kmalloc(sizeof(*uddev));

    if (!uddev)
        return -KERR_NOMEM;

    /* Set the usb disk device fields */
    uddev->udev = dev;

    /* Initialize generic volume info */
    usbdisk_init_volume_dev_info(uddev, &info);

    /* Finally, create the volume device */
    uddev->diskdev = create_volume_device(&info, &__usbdisk_ops, NULL, uddev);

    /* Register the generic disk device (Because idk) */
    register_volume_device(uddev->diskdev);

    /* TODO: Register the disk device to oss (Should gdisk do this?) */
    return 0;
}

int usbdisk_destroy(struct usbdisk_dev* dev)
{
    /* Murder the volume device */
    destroy_volume_device(dev->diskdev);

    /* Also murder the usb device */
    destroy_usb_device(dev->udev);

    /* Finally, murder our own memory (Gosh we're cruel) */
    kfree(dev);
    return 0;
}
