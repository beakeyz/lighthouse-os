#include "dev/manifest.h"
#include <dev/core.h>
#include <dev/ctl.h>
#include <dev/driver.h>
/* Usb includes */
#include <dev/usb/driver.h>
#include <dev/usb/usb.h>

usb_device_ident_t usbdisk_ident[] = {
    /* ATAPI */
    USB_DEV_IDENT(0, 0, 0x8, 0x2, 0, 0),
    /* UFI */
    USB_DEV_IDENT(0, 0, 0x8, 0x4, 0, 0),
    /* ATAPI */
    USB_DEV_IDENT(0, 0, 0x8, 0x5, 0, 0),
    /* We target generic SCSI usb disk devices */
    USB_DEV_IDENT(0, 0, 0x8, 0x6, 0, 0),
    /* BULK only */
    USB_DEV_IDENT(0, 0, 0x8, 0x50, 0, 0),
    USB_END_IDENT,
};

struct usbdisk_dev;

extern int usbdisk_create(drv_manifest_t* driver, usb_device_t* dev, usb_interface_buffer_t* intrf);
extern int usbdisk_destroy(struct usbdisk_dev* dev);

/*!
 * @brief: Checks if we are able to drive this device lol
 */
static int usbdisk_probe(drv_manifest_t* driver, usb_device_t* dev, usb_interface_buffer_t* intrf)
{
    return usbdisk_create(driver, dev, intrf);
}

/*!
 * @brief: Remove a usbdisk device
 */
static int usbdisk_remove(drv_manifest_t* driver, usb_device_t* dev)
{
    return usbdisk_destroy(dev->private);
}

usb_driver_desc_t usbdisk_drv = {
    .f_probe = usbdisk_probe,
    .f_remove = usbdisk_remove,
    .ident_list = usbdisk_ident,
};

/*!
 * @brief: Initialize the driver infastructure
 *
 */
static int usbdisk_init(drv_manifest_t* driver)
{
    /* Register the usb driver and collect the devices */
    register_usb_driver(driver, &usbdisk_drv);
    return 0;
}

/*!
 * @brief: Tear down the driver structure
 */
static int usbdisk_exit()
{
    /* Remove the usb driver and release and clean all devices */
    unregister_usb_driver(&usbdisk_drv);
    return 0;
}

EXPORT_DRIVER(usbdisk) = {
    .m_name = "usbdisk",
    .m_descriptor = "Generic USB block device driver",
    .m_type = DT_DISK,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = usbdisk_init,
    .f_exit = usbdisk_exit,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};
