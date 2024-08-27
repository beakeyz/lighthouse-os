#include "dev/device.h"
#include "dev/disk/generic.h"
#include "dev/usb/usb.h"

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

device_ctl_node_t usbdisk_ctls[] = {
    DEVICE_CTL(DEVICE_CTLC_DISK_BREAD, usbdisk_bread, NULL),
    DEVICE_CTL(DEVICE_CTLC_DISK_BWRITE, usbdisk_bwrite, NULL),
    DEVICE_CTL_END,
};
