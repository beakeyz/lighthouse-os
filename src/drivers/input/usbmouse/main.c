#include "dev/io/hid/hid.h"
#include "dev/usb/spec.h"
#include "dev/usb/xfer.h"
#include "logging/log.h"
#include "mem/heap.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/driver.h>

#include <dev/usb/driver.h>
#include <dev/usb/usb.h>

static usb_device_ident_t usbmouse_ident[] = {
    USB_DEV_IDENT(0, 0, 3, 1, 2, 0),
    USB_END_IDENT,
};

struct usbmouse_bootpacket {
    u8 button_flags;
    i8 x;
    i8 y;
    i8 z;
    i8 reserved1;
    i8 reserved2;
} __attribute__((packed));

struct usbmouse {
    hid_device_t* hdev;
    usb_device_t* udev;
    usb_interface_buffer_t* interf;
    usb_xfer_t* probe_xfer;

    union {
        struct usbmouse_bootpacket packet;
        u8 raw_buffer[8];
    } resp;

    struct usbmouse* next;
};

static u32 _next_usbmouse_id;
static struct usbmouse* _usbmice;

static struct usbmouse* getusbmouse_for_udev(usb_device_t* udev)
{
    for (struct usbmouse* mouse = _usbmice; mouse; mouse = mouse->next) {
        if (mouse->udev == udev)
            return mouse;
    }

    return nullptr;
}

static int usbmouse_irq(usb_xfer_t* xfer)
{
    struct usbmouse* mouse;

    /* Fuck */
    if (!xfer || usb_xfer_is_err(xfer))
        return -1;

    mouse = xfer->priv_ctx;

    KLOG("Recieved USB mouse packet (x:%d, y:%d)\n", mouse->resp.packet.x, -mouse->resp.packet.y);

    /* Resubmit */
    usb_xfer_enqueue(xfer, xfer->device->hcd);
    return 0;
}

static struct usbmouse* create_and_link_usbmouse(driver_t* driver, usb_device_t* dev, usb_interface_buffer_t* interf)
{
    struct usbmouse* ret;
    usb_endpoint_buffer_t* ep;
    char devname[28] = { 0 };

    if (!interf || !interf->alt_list)
        return nullptr;

    ep = interf->alt_list->ep_list;

    if (!ep)
        return nullptr;

    ret = kmalloc(sizeof(*ret));

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    sfmt(devname, "usbmouse_%d", _next_usbmouse_id++);
    ret->udev = dev;
    ret->interf = interf;

    if (usb_device_submit_int(dev, &ret->probe_xfer, usbmouse_irq, USB_DIRECTION_DEVICE_TO_HOST, interf->alt_list->ep_list, &ret->resp.packet, sizeof(ret->resp))) {
        kfree(ret);
        return nullptr;
    }

    /* Set the private context */
    ret->probe_xfer->priv_ctx = ret;

    /* Create the hid device for this mouse */
    ret->hdev = create_hid_device(driver, devname, HID_BUS_TYPE_USB, NULL);

    if (!ret->hdev) {
        release_usb_xfer(ret->probe_xfer);
        kfree(ret);
        return nullptr;
    }

    /* Yaya */
    register_hid_device(ret->hdev);

    /* link this shit */
    ret->next = _usbmice;
    _usbmice = ret;

    return ret;
}

static int usbmouse_probe(driver_t* driver, usb_device_t* device, usb_interface_buffer_t* interf)
{
    struct usbmouse* usbmouse;
    usb_interface_entry_t* interface;
    KLOG_DBG("Found a usb mouse device!\n");

    interface = interf->alt_list;

    if (!interface)
        return -KERR_NULL;

    /* Can't do this crap on interfaces with more than one endpoints */
    if (interface->desc.num_endpoints != 1)
        return -KERR_DEV;

    /* Endpoint has to be incomming */
    if (!usb_endpoint_dir_is_inc(interface->ep_list))
        return -KERR_DEV;

    /* Endpoint has to be of the interrupt type */
    if (!usb_endpoint_type_is_int(interface->ep_list))
        return -KERR_DEV;

    usbmouse = create_and_link_usbmouse(driver, device, interf);

    if (!usbmouse)
        return -KERR_NODEV;

    /* Enqueue the probe xfer */
    usb_xfer_enqueue(usbmouse->probe_xfer, device->hcd);

    return 0;
}

usb_driver_desc_t usbmouse_drv = {
    .f_probe = usbmouse_probe,
    .ident_list = usbmouse_ident,
};

static int usbmouse_init(driver_t* driver)
{
    _usbmice = nullptr;
    _next_usbmouse_id = 0;

    register_usb_driver(driver, &usbmouse_drv);
    return 0;
}

static int usbmouse_exit()
{
    unregister_usb_driver(&usbmouse_drv);
    return 0;
}

EXPORT_DRIVER(usbmouse) = {
    .m_name = "usbmouse",
    .m_descriptor = "USB mouse boot protocol driver",
    .m_version = DRIVER_VERSION(0, 0, 1),
    .m_type = DT_IO,
    .f_init = usbmouse_init,
    .f_exit = usbmouse_exit,
};

EXPORT_DEPENDENCIES(_deps) = {
    DRV_DEP_END,
};
