#ifndef __ANIVA_USB_DEVICE_DRIVER__
#define __ANIVA_USB_DEVICE_DRIVER__

#include "dev/driver.h"
#include <libk/stddef.h>

struct driver;
struct usb_device;
struct usb_interface_buffer;

typedef struct usb_device_ident {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    uint8_t if_num;
} usb_device_ident_t;

#define USB_DEV_IDENT(vid, did, class, subclass, protocol, if_num) \
    {                                                              \
        (vid),                                                     \
        (did),                                                     \
        (class),                                                   \
        (subclass),                                                \
        (protocol),                                                \
        (if_num),                                                  \
    }
#define USB_END_IDENT \
    {                 \
        0,            \
        0,            \
        0,            \
        0,            \
        0,            \
        0,            \
    }

/*!
 * @brief: Static descriptor for a usb device driver
 */
typedef struct usb_driver_desc {
    /* Array of USB identifiers. Should end with an all-zero entry */
    struct usb_device_ident* ident_list;

    int (*f_probe)(struct driver* driver, struct usb_device* device, struct usb_interface_buffer* interface);
    int (*f_remove)(struct driver* driver, struct usb_device* device);
} usb_driver_desc_t;

int register_usb_driver(driver_t* driver, usb_driver_desc_t* desc);
int unregister_usb_driver(usb_driver_desc_t* desc);

int usbdrv_device_scan_driver(struct usb_device* udev);

void init_usb_drivers();
#endif
