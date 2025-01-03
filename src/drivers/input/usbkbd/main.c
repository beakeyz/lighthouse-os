#include <dev/core.h>
#include <dev/driver.h>
#include <dev/usb/driver.h>

#include "dev/io/hid/event.h"
#include "dev/io/hid/hid.h"
#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "lightos/dev/shared.h"
#include "drivers/input/i8042/i8042.h"
#include "libk/flow/error.h"
#include "libk/math/math.h"
#include "libk/stddef.h"
#include "lightos/event/key.h"
#include "logging/log.h"
#include "mem/heap.h"

usb_device_ident_t usbkbd_ident_list[] = {
    USB_DEV_IDENT(0, 0, 3, 1, 1, 0),
    USB_END_IDENT
};

#define USBKBD_MOD_RSHIFT 0x0001
#define USBKBD_MOD_LSHIFT 0x0002
#define USBKBD_MOD_RALT 0x0004
#define USBKBD_MOD_LALT 0x0008
#define USBKBD_MOD_LCTL 0x0010
#define USBKBD_MOD_RCTL 0x0020
#define USBKBD_MOD_HOME 0x0040
#define USBKBD_MOD_CAPSLOCK 0x0080
#define USBKBD_MOD_TAB 0x0100
#define USBKBD_MOD_RETURN 0x0200
#define USBKBD_MOD_ESC 0x0400

typedef struct usbkbd {
    usb_device_t* dev;
    hid_device_t* hiddev;
    usb_xfer_t* probe_xfer;

    uint8_t this_resp[8];
    uint8_t prev_resp[8];
    uint16_t mod_keys;
    uint16_t c_scancodes[7];

    /* How many times we've polled the device until now */
    size_t nr_poll_calls;

    /* The manufacurer-given name of this device */
    const char* device_name;

    hid_event_t c_ctx;

    struct usbkbd* next;
} usbkbd_t;

static u32 n_keyboards;
static usbkbd_t* keyboards;

/*!
 * Yoinked from: https://github.com/torvalds/linux/blob/master/drivers/hid/usbhid/usbkbd.c
 */
static const unsigned char usb_kbd_keycode[256] = {
    0, 0, 0, 0, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38,
    50, 49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44, 2, 3,
    4, 5, 6, 7, 8, 9, 10, 11, 28, 1, 14, 15, 57, 12, 13, 26,
    27, 43, 43, 39, 40, 41, 51, 52, 53, 58, 59, 60, 61, 62, 63, 64,
    65, 66, 67, 68, 87, 88, 99, 70, 119, 110, 102, 104, 111, 107, 109, 106,
    105, 108, 103, 69, 98, 55, 74, 78, 96, 79, 80, 81, 75, 76, 77, 71,
    72, 73, 82, 83, 86, 127, 116, 117, 183, 184, 185, 186, 187, 188, 189, 190,
    191, 192, 193, 194, 134, 138, 130, 132, 128, 129, 131, 137, 133, 135, 136, 113,
    115, 114, 0, 0, 0, 121, 0, 89, 93, 124, 92, 94, 95, 0, 0, 0,
    122, 123, 90, 91, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    29, // Tab
    42, // LShift
    56, // Lalt
    125, // ??
    97, // ??
    54, // RShift
    100, // ??
    126, // ??
    164, // ??
    166, 165, 163, 161, 115, 114, 113,
    150, 158, 159, 128, 136, 177, 178, 176, 142, 152, 173, 140
};

static int get_usbkbd(usbkbd_t** p_kbd, usb_device_t* udev, hid_device_t* hdev)
{
    usbkbd_t* ret;

    ret = nullptr;

    for (ret = keyboards; ret; ret = ret->next) {
        if ((udev && ret->dev == udev) || (hdev && ret->hiddev == hdev))
            break;
    }

    *p_kbd = ret;
    return 0;
}

static int usbkbd_disable(device_t* device)
{
    usbkbd_t* kbd;
    hid_device_t* hid;

    if (!device)
        return -1;

    hid = device->private;

    if (get_usbkbd(&kbd, NULL, hid))
        return -1;

    if (kbd->probe_xfer)
        usb_xfer_cancel(kbd->probe_xfer);

    return 0;
}

static int usbkbd_enable(device_t* device)
{
    usbkbd_t* kbd;
    hid_device_t* hid;

    if (!device)
        return -1;

    hid = device->private;

    if (get_usbkbd(&kbd, NULL, hid))
        return -1;

    /* Reenqueue the probing transfer to enable the device */
    if (kbd->probe_xfer && usb_xfer_is_done(kbd->probe_xfer))
        usb_xfer_enqueue(kbd->probe_xfer, kbd->dev->hcd);

    return 0;
}

static int usbkbd_getinfo(device_t* dev, driver_t* drv, uintptr_t offset, void* buffer, size_t size)
{
    DEVINFO* pDevinfo;
    hid_device_t* hiddev;
    usbkbd_t* kbd;

    if (size != sizeof(*pDevinfo))
        return -KERR_INVAL;

    /* Grab the devinfo buffer */
    pDevinfo = (DEVINFO*)buffer;
    hiddev = (hid_device_t*)dev->private;

    if (!pDevinfo || !hiddev)
        return -KERR_INVAL;

    if (get_usbkbd(&kbd, nullptr, hiddev))
        return -KERR_INVAL;

    pDevinfo->class = dev->class;
    pDevinfo->subclass = dev->subclass;
    pDevinfo->ctype = dev->type;
    pDevinfo->deviceid = dev->device_id;
    pDevinfo->vendorid = dev->vendor_id;

    if (kbd->device_name)
        memcpy(pDevinfo->devicename, kbd->device_name, MIN(sizeof(pDevinfo->devicename) - 1, strlen(kbd->device_name)));

    /* Let the broski know how many times we've polled until now */
    if (pDevinfo->dev_specific_info && pDevinfo->dev_specific_size == sizeof(size_t))
        *(size_t*)pDevinfo->dev_specific_info = kbd->nr_poll_calls;

    return 0;
}

static device_ctl_node_t _usbkbd_ctllist[] = {
    DEVICE_CTL(DEVICE_CTLC_ENABLE, usbkbd_enable, NULL),
    DEVICE_CTL(DEVICE_CTLC_DISABLE, usbkbd_disable, NULL),
    DEVICE_CTL(DEVICE_CTLC_GETINFO, usbkbd_getinfo, NULL),
    DEVICE_CTL_END,
};

/*!
 * @brief: Create a new usb keyboard device
 *
 * Registers the new device to our keyboard device list
 */
static int
create_usbkbd(usbkbd_t** ret, driver_t* usbkbd_driver, usb_device_t* dev)
{
    usbkbd_t* _ret;
    char name_buffer[16] = { 0 };

    /* TEST: only have one keyboard for now */
    if (keyboards)
        return -KERR_DEV;

    ASSERT(hid_disable_i8042() == 0);

    _ret = kmalloc(sizeof(*_ret));

    if (!_ret)
        return -1;

    memset(_ret, 0, sizeof(*_ret));

    /* Format the device name correctly */
    sfmt(name_buffer, "usbkbd_%d", n_keyboards);

    _ret->dev = dev;
    _ret->hiddev = create_hid_device(usbkbd_driver, name_buffer, HID_BUS_TYPE_USB, _usbkbd_ctllist);

    if (!_ret->hiddev) {
        kfree(_ret);
        return -KERR_NOMEM;
    }

    /* Register the hid device so we can use it externaly */
    register_hid_device(_ret->hiddev);

    /* Prepare the hid context */
    _ret->c_ctx.type = HID_EVENT_KEYPRESS;
    _ret->c_ctx.device = _ret->hiddev;

    /* Grab the device name */
    _ret->device_name = "TODO: Get kb name";

    /* Link it to our list */
    _ret->next = keyboards;
    keyboards = _ret;

    n_keyboards++;

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

    /* Make sure the transfer is canceled */
    if (kbd->probe_xfer)
        usb_xfer_cancel(kbd->probe_xfer);

    /* Release our reference of the transfer */
    release_usb_xfer(kbd->probe_xfer);

    /* Buhbye */
    kfree(kbd);
    return 0;
}

static inline void usbkbd_set_keycode_buffer(usbkbd_t* kbd, uint16_t keycode, bool pressed)
{
    uint32_t key_idx;

    if (pressed) {
        for (key_idx = 0; key_idx < arrlen(kbd->c_scancodes); key_idx++) {
            /* No duplicates */
            if (kbd->c_scancodes[key_idx] == keycode)
                break;

            /* Skip taken scancodes */
            if (kbd->c_scancodes[key_idx])
                continue;

            kbd->c_scancodes[key_idx] = keycode;
            break;
        }

        return;
    }

    key_idx = 0;

    /* Search for our keycode */
    for (; key_idx < arrlen(kbd->c_scancodes); key_idx++)
        if (kbd->c_scancodes[key_idx] == keycode)
            break;

    /* Shift the remaining keys forwards */
    while (key_idx < arrlen(kbd->c_scancodes)) {

        /* If we can't, simply put a NULL */
        if ((key_idx + 1) >= arrlen(kbd->c_scancodes))
            kbd->c_scancodes[key_idx] = NULL;
        else
            kbd->c_scancodes[key_idx] = kbd->c_scancodes[key_idx + 1];

        key_idx++;
    }
}

static inline void usbkbd_set_mod(usbkbd_t* kbd, uint16_t flag, bool pressed)
{
    if (pressed)
        kbd->mod_keys |= flag;
    else
        kbd->mod_keys &= ~flag;
}

static int usbkbd_fire_key(usbkbd_t* kbd, uint16_t keycode, bool pressed)
{
    if (pressed)
        kbd->c_ctx.key.flags |= HID_EVENT_KEY_FLAG_PRESSED;
    else
        kbd->c_ctx.key.flags &= ~HID_EVENT_KEY_FLAG_PRESSED;

    kbd->c_ctx.key.scancode = aniva_scancode_table[keycode];
    kbd->c_ctx.key.pressed_char = (kbd->mod_keys & (USBKBD_MOD_LSHIFT | USBKBD_MOD_RSHIFT)) ? kbd_us_shift_map[keycode] : kbd_us_map[keycode];

    switch (kbd->c_ctx.key.scancode) {
    case ANIVA_SCANCODE_LSHIFT:
        usbkbd_set_mod(kbd, USBKBD_MOD_LSHIFT, pressed);
        break;
    case ANIVA_SCANCODE_RSHIFT:
        usbkbd_set_mod(kbd, USBKBD_MOD_RSHIFT, pressed);
        break;
    case ANIVA_SCANCODE_TAB:
        usbkbd_set_mod(kbd, USBKBD_MOD_TAB, pressed);
        break;
    default:
        break;
    }

    // KLOG("usbkbk: %s %s\n", pressed ? "Pressed" : "Released", (u8[]) { kbd->c_ctx.key.pressed_char, 0 });

    usbkbd_set_keycode_buffer(kbd, kbd->c_ctx.key.scancode, pressed);

    /* Copy the scancode buffer to the event */
    memcpy(&kbd->c_ctx.key.pressed_buffer, kbd->c_scancodes, 7);

    /* Queue into the device event queue */
    hid_device_queue(kbd->hiddev, &kbd->c_ctx);
    return 0;
}

static inline bool has_pressed_key(uint8_t keys[6], uint8_t key)
{
    for (uint8_t i = 0; i < 6; i++)
        if (keys[i] == key)
            return true;

    return false;
}

/*!
 * @brief: IRQ callback for the usb device polling routine
 *
 * Most of this idea is yoinked from linux xD
 */
static int usbkbd_irq(usb_xfer_t* xfer)
{
    usbkbd_t* kbd;
    uint8_t i;

    kbd = xfer->priv_ctx;

    // KLOG_INFO("USBKBD: Recieved!\n");

    /* This sucks balls */
    if (!kbd)
        return -1;

    /* Add a poll count */
    kbd->nr_poll_calls++;

    if (usb_xfer_is_err(xfer))
        goto resubmit;

    /* No data transfered (Probably a NAK) */
    if (xfer->resp_transfer_size == 0)
        goto resubmit;

    /* If both buffers are the same, nothing changed */
    if (memcmp(kbd->this_resp, kbd->prev_resp, sizeof(kbd->this_resp)) == true)
        goto resubmit;

    // KLOG("(%x %x %x %x) -> (%x %x %x %x)\n", kbd->prev_resp[2], kbd->prev_resp[3], kbd->prev_resp[4], kbd->prev_resp[5],
    // kbd->this_resp[2], kbd->this_resp[3], kbd->this_resp[4], kbd->this_resp[5]);

    // goto resubmit;

    /* Check for key releases */
    for (i = 2; i < 8; i++)
        /* Check if there was a difference in a previous input in regard to the current input to check key releases */
        if (kbd->prev_resp[i] > 3 && !has_pressed_key(kbd->this_resp + 2, kbd->prev_resp[i]))
            usbkbd_fire_key(kbd, usb_kbd_keycode[kbd->prev_resp[i]], false);

    /* Check modifier keys */
    for (i = 0; i < 8; i++) {
        if (((kbd->this_resp[0] >> i) & 1) == 1 && ((kbd->prev_resp[0] >> i) & 1) == 0)
            usbkbd_fire_key(kbd, usb_kbd_keycode[224 + i], true);

        if (((kbd->this_resp[0] >> i) & 1) == 0 && ((kbd->prev_resp[0] >> i) & 1) == 1)
            usbkbd_fire_key(kbd, usb_kbd_keycode[224 + i], false);
    }

    /* Check for key presses */
    for (i = 2; i < 8; i++)
        /* The exact reverse for keypresses */
        if (kbd->this_resp[i] > 3 && !has_pressed_key(kbd->prev_resp + 2, kbd->this_resp[i]))
            usbkbd_fire_key(kbd, usb_kbd_keycode[kbd->this_resp[i]], true);

    /* Copy the previous key response into a new buffer */
    memcpy(kbd->prev_resp, kbd->this_resp, sizeof(kbd->prev_resp));
resubmit:
    /* Resubmit this xfer to the hcd in order to keep probing the device */
    return usb_xfer_enqueue(xfer, xfer->device->hcd);
}

static int usbkbd_probe(driver_t* this, usb_device_t* udev, usb_interface_buffer_t* intf_buf)
{
    int error;
    usb_interface_entry_t* interface;

    usbkbd_t* kbd;

    if (!intf_buf)
        return -KERR_NULL;

    interface = intf_buf->alt_list;

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

    error = create_usbkbd(&kbd, this, udev);

    if (error)
        return error;

    /* Select the boot protocol, which we use */
    error = usb_device_submit_ctl(udev, 0x21, USB_REQ_SET_PROTOCOL, 0, 0, 0, NULL, NULL);

    if (error)
        goto dealloc_and_exit;

    /* Submit an interrupt transfer */
    error = usb_device_submit_int(udev, &kbd->probe_xfer, usbkbd_irq, USB_DIRECTION_DEVICE_TO_HOST, interface->ep_list, kbd->this_resp, sizeof(kbd->this_resp));

    if (error)
        goto dealloc_and_exit;

    /* Set the xfer context */
    kbd->probe_xfer->priv_ctx = kbd;

    return 0;
dealloc_and_exit:
    destroy_usbkbd(kbd);
    return error;
}

usb_driver_desc_t usbkbd_usbdrv = {
    .ident_list = usbkbd_ident_list,
    .f_probe = usbkbd_probe,
};

static int usbkbd_init(driver_t* driver)
{
    KLOG_DBG("Initializing usbkbd\n");

    keyboards = nullptr;
    n_keyboards = NULL;

    /* Register the usb driver */
    register_usb_driver(driver, &usbkbd_usbdrv);
    return 0;
}

static int usbkbd_exit()
{
    KLOG_DBG("Exiting usbkbd\n");

    while (keyboards)
        destroy_usbkbd(keyboards);

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
