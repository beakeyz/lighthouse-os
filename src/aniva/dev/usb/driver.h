#ifndef __ANIVA_USB_DEVICE_DRIVER__
#define __ANIVA_USB_DEVICE_DRIVER__

#include "dev/manifest.h"
#include <libk/stddef.h>

struct drv_manifest;
struct usb_device;

typedef struct usb_device_ident {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t class;
    uint16_t subclass;
} usb_device_ident_t;

#define USB_DEV_IDENT(vid, did, class, subclass) \
    {                                            \
        (vid),                                   \
        (did),                                   \
        (class),                                 \
        (subclass),                              \
    }
#define USB_END_IDENT \
    {                 \
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

    int (*f_probe)(struct drv_manifest* driver, struct usb_device* device);
    int (*f_remove)(struct drv_manifest* driver, struct usb_device* device);
} usb_driver_desc_t;

int register_usb_driver(drv_manifest_t* driver, usb_driver_desc_t* desc);
int unregister_usb_driver(usb_driver_desc_t* desc);

void init_usb_drivers();
#endif
