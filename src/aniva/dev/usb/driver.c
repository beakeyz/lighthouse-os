#include "driver.h"
#include "dev/manifest.h"
#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/heap.h"

list_t* usbdrv_list;

typedef struct usb_driver {
    usb_driver_desc_t* desc;
    /* The core driver associated with this usb driver */
    drv_manifest_t* driver;

    /* List where we keep all devices attached to this driver through USB */
    list_t* usb_devices;
} usb_driver_t;

static inline bool _usb_if_matches_ident(usb_device_t* dev, usb_device_ident_t* ident, usb_interface_entry_t* intf)
{
    if (ident->class && ident->class != intf->desc.interface_class)
        return false;
    if (ident->subclass && ident->subclass != intf->desc.interface_subclass)
        return false;
    if (ident->protocol && ident->protocol != intf->desc.interface_protocol)
        return false;
    if (ident->if_num && ident->if_num != intf->desc.interface_number)
        return false;
    if (ident->vendor_id && ident->vendor_id != dev->device->vendor_id)
        return false;
    if (ident->device_id && ident->device_id != dev->device->device_id)
        return false;

    return true;
}

static inline bool _matching_device(usb_device_ident_t* idents, usb_device_t* device, usb_interface_buffer_t** matching_if_buff)
{
    uint32_t idx = 0;
    uint32_t if_idx;
    usb_config_buffer_t cfg_buffer;
    usb_interface_buffer_t* c_if_buffer;
    usb_interface_entry_t* c_interface;

    usb_device_get_active_config(device, &cfg_buffer);

    /* 0-0-0-0-0-0 means end of identifiers */
    while (idents[idx].class || idents[idx].subclass || idents[idx].protocol || idents[idx].if_num || idents[idx].device_id || idents[idx].vendor_id) {

        for (if_idx = 0; if_idx < cfg_buffer.if_count; if_idx++) {
            c_if_buffer = &cfg_buffer.if_arr[if_idx];

            c_interface = c_if_buffer->alt_list;

            for (; c_interface != nullptr; c_interface = c_interface->next) {
                KLOG_DBG("Checking interface %d:%d:%d to ident %d:%d:%d\n",
                    c_interface->desc.interface_class, c_interface->desc.interface_subclass, c_interface->desc.interface_protocol,
                    idents[idx].class, idents[idx].subclass, idents[idx].protocol);

                if (!_usb_if_matches_ident(device, &idents[idx], c_interface))
                    continue;

                /* Export the matching interface */
                if (matching_if_buff)
                    *matching_if_buff = c_if_buffer;
                return true;
            }
        }
        idx++;
    }

    return false;
}

/*!
 * @brief: Scan the driver list for a compatible driver for @udev
 *
 */
int usbdrv_device_scan_driver(usb_device_t* udev)
{
    int error;
    usb_driver_t* c_driver;
    usb_interface_buffer_t* matching_if;

    matching_if = nullptr;
    c_driver = nullptr;

    FOREACH(i, usbdrv_list)
    {
        c_driver = i->data;

        /* Already probed this fucker, skip */
        if (manifest_has_dev(c_driver->driver, udev->device, NULL))
            goto cycle;

        if (!_matching_device(c_driver->desc->ident_list, udev, &matching_if) || !c_driver->desc->f_probe)
            goto cycle;

        /* Do the probe */
        error = c_driver->desc->f_probe(c_driver->driver, udev, matching_if);

        /* Yay we've found our device */
        if (KERR_OK(error))
            break;

    cycle:
        c_driver = nullptr;
    }

    if (!c_driver)
        return -KERR_NODRV;

    /* Give the driver a device */
    manifest_add_dev(c_driver->driver, udev->device);

    /* Attach the usb device */
    list_append(c_driver->usb_devices, udev);
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
    drv->usb_devices = init_list();

    /* Add to the list */
    list_append(usbdrv_list, drv);

    /* Do a device scan */
    usbdrv_do_device_scan();
    return 0;
}

/*!
 * @brief: Unregister a USB driver through its descriptor
 *
 * Removes the driver from the driverlist, so it won't get included anymore in the
 * probe sequence. This also removes any devices this driver has created
 */
int unregister_usb_driver(usb_driver_desc_t* desc)
{
    int error;
    usb_device_t* device;
    usb_driver_t* driver;

    if (!desc)
        return -KERR_INVAL;

    driver = 0;
    error = 0;

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

    /* Check if this driver even provides a 'remove' option */
    if (!driver->desc->f_remove)
        goto remove_and_free;

    /* Remove all devices */
    FOREACH(i, driver->usb_devices)
    {
        device = i->data;

        /* Remove this device */
        error = driver->desc->f_remove(driver->driver, device);

        /* This sucks man */
        if (error)
            break;
    }

remove_and_free:
    list_remove_ex(usbdrv_list, driver);
    destroy_list(driver->usb_devices);
    kfree(driver);
    return error;
}

void init_usb_drivers()
{
    usbdrv_list = init_list();
}
