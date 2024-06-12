#include <dev/core.h>
#include <dev/manifest.h>

#include "dev/debug/serial.h"
#include "dev/usb/driver.h"
#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "logging/log.h"
#include "mem/heap.h"

usb_device_ident_t usbkbd_ident_list[] = {
    USB_DEV_IDENT(0, 0, 3, 1, 1, 0),
    USB_END_IDENT
};

typedef struct usbkbd {
    usb_device_t* dev;
    usb_xfer_t* probe_xfer;

    uint8_t this_resp[8];
    uint8_t prev_resp[8];

    struct usbkbd* next;
} usbkbd_t;

static usbkbd_t* keyboards;

/*!
 * @brief: Create a new usb keyboard device
 *
 * Registers the new device to our keyboard device list
 */
static int create_usbkbd(usbkbd_t** ret, usb_device_t* dev)
{
    usbkbd_t* _ret;

    _ret = kmalloc(sizeof(*_ret));

    if (!_ret)
        return -1;

    _ret->dev = dev;

    /* Link it to our list */
    _ret->next = keyboards;
    keyboards = _ret;

    if (ret)
        *ret = _ret;
    return 0;
}

static int destroy_usbkbd(usbkbd_t* kbd)
{
    usbkbd_t** c_kbd;

    if (!kbd)
        return -KERR_INVAL;

    /* Unlink */
    for (c_kbd = &keyboards; *c_kbd; c_kbd = &(*c_kbd)->next) {
        if (*c_kbd == kbd) {
            *c_kbd = kbd->next;
            break;
        }
    }

    /* Buhbye */
    kfree(kbd);
    return 0;
}

static int get_usbkbd(usbkbd_t** p_kbd, usb_device_t* udev)
{
    usbkbd_t* ret;

    for (ret = keyboards; ret; ret = ret->next) {
        if (ret->dev == udev)
            break;
    }

    *p_kbd = ret;
    return -(int)(ret == nullptr);
}

static int usbkbd_irq(usb_xfer_t* xfer)
{
    int error;
    usbkbd_t* kbd;

    error = get_usbkbd(&kbd, xfer->device);

    if (error)
        goto resubmit;

    serial_println("Got a response from usbkbd =D");

resubmit:
    /* Resubmit this xfer to the hcd in order to keep probing the device */
    usb_xfer_enqueue(xfer, xfer->device->hcd);
    return 0;
}

static int usbkbd_probe(drv_manifest_t* this, usb_device_t* udev, usb_interface_buffer_t* interface)
{
    int error;
    usbkbd_t* kbd;

    error = create_usbkbd(&kbd, udev);

    if (error)
        return error;

    /* Submit an interrupt transfer */
    error = usb_device_submit_int(udev, &kbd->probe_xfer, usbkbd_irq, USB_DIRECTION_DEVICE_TO_HOST, interface->ep_arr, kbd->this_resp, sizeof(kbd->this_resp));

    return error;
}

usb_driver_desc_t usbkbd_usbdrv = {
    .ident_list = usbkbd_ident_list,
    .f_probe = usbkbd_probe,
};

static int usbkbd_init(drv_manifest_t* driver)
{
    KLOG_DBG("Initializing usbkbd\n");

    keyboards = nullptr;

    /* Register the usb driver */
    register_usb_driver(driver, &usbkbd_usbdrv);
    return 0;
}

static int usbkbd_exit()
{
    KLOG_DBG("Exiting usbkbd\n");

    /* Aaaand remove the driver */
    unregister_usb_driver(&usbkbd_usbdrv);
    return 0;
}

/*
 * Core driver for any USB keyboard device that might be present on the system
 *
 * The core driver registers a USB driver, which will perform the bindings to
 * the USB devices. The registering the USB driver will initiate a bus probe
 * from this driver, which will try to find any devices on the system that the
 * driver is able to talk with. After registering, any new devices that are
 * either discovered or connected will get examined by the driver. It is the
 * job of this driver to implement 'input' endpoints for any keyboard devices
 * found, so userspace can talk with the devices in an expected way
 */
EXPORT_DRIVER(usbkbd_driver) = {
    .m_name = "usbkbd",
    .m_descriptor = "Default USB keyboard driver",
    .m_type = DT_IO,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = usbkbd_init,
    .f_exit = usbkbd_exit,
};

/* This driver has no dependencies */
EXPORT_EMPTY_DEPENDENCIES(deps);
