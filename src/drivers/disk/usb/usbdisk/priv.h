#ifndef __ANIVA_USBDISK_PRIV__
#define __ANIVA_USBDISK_PRIV__

#include "dev/driver.h"
#include "dev/usb/usb.h"

struct usbdisk_dev;

extern int usbdisk_create(driver_t* driver, usb_device_t* dev, usb_interface_buffer_t* intrf);
extern int usbdisk_destroy(struct usbdisk_dev* dev);

#endif // !__ANIVA_USBDISK_PRIV__
