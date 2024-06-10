#include "driver.h"
#include "dev/manifest.h"
#include "dev/usb/usb.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"

list_t* usbdrv_list;

typedef struct usb_driver {
    usb_driver_desc_t* desc;
    /* The core driver associated with this usb driver */
    drv_manifest_t* driver;
} usb_driver_t;

static inline bool _matching_device(usb_device_ident_t* idents, device_t* device)
{
    uint32_t idx = 0;

    /* 0-0-0-0 means end of identifiers */
    while (idents[idx].class || idents[idx].subclass || idents[idx].device_id || idents[idx].vendor_id) {
        if (idents[idx].class && idents[idx].class != device->class)
            goto cycle;
        if (idents[idx].subclass && idents[idx].subclass != device->subclass)
            goto cycle;
        if (idents[idx].device_id && idents[idx].device_id != device->device_id)
            goto cycle;
        if (idents[idx].vendor_id && idents[idx].vendor_id != device->vendor_id)
            goto cycle;

        return true;

    cycle:
        idx++;
    }

    return false;
}

/*!
 * @brief: Scan the driver list for a compatible driver for @udev
 *
 */
static int usbdrv_device_scan_driver(usb_device_t* udev)
{
    int error;
    usb_driver_t* c_driver;

    c_driver = nullptr;

    FOREACH(i, usbdrv_list)
    {
        c_driver = i->data;

        if (!_matching_device(c_driver->desc->ident_list, udev->device) || !c_driver->desc->f_probe)
            continue;

        /* Do the probe */
        error = c_driver->desc->f_probe(c_driver->driver, udev);

        /* Yay we've found our device */
        if (KERR_OK(error))
            break;

        c_driver = nullptr;
    }

    if (!c_driver)
        return -KERR_NODRV;

    /* Give the driver a device */
    manifest_add_dev(c_driver->driver, udev->device);
    return 0;
}

static void __usbdrv_do_device_scan(struct usb_device* device, void* param)
{
    if (!device)
        return;

    (void)usbdrv_device_scan_driver(device);
}

/*!
 * @brief: Enumerate the usb devices to see if we have matches
 */
static inline int usbdrv_do_device_scan()
{
    return enumerate_usb_devices(__usbdrv_do_device_scan, NULL);
}

int register_usb_driver(drv_manifest_t* driver, usb_driver_desc_t* desc)
{
    usb_driver_t* drv;

    if (!desc)
        return -KERR_INVAL;

    /* Check if we already have this driver */
    FOREACH(i, usbdrv_list)
    {
        drv = i->data;

        if (drv->desc == desc)
            return -KERR_DUPLICATE;
    }

    /* Allocate this driver */
    drv = kmalloc(sizeof(*drv));

    if (!drv)
        return -1;

    drv->driver = driver;
    drv->desc = desc;

    /* Add to the list */
    list_append(usbdrv_list, drv);

    /* Do a device scan */
    usbdrv_do_device_scan();
    return 0;
}

int unregister_usb_driver(usb_driver_desc_t* desc)
{
    usb_driver_t* driver;

    if (!desc)
        return -KERR_INVAL;

    driver = 0;

    /* Find the correct driver object */
    FOREACH(i, usbdrv_list)
    {
        driver = i->data;

        if (driver->desc == desc)
            break;

        driver = nullptr;
    }

    if (!driver)
        return -1;

    list_remove_ex(usbdrv_list, driver);
    kfree(driver);
    return 0;
}

void init_usb_drivers()
{
    usbdrv_list = init_list();
}
