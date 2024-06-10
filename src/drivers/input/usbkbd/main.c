#include <dev/core.h>
#include <dev/manifest.h>

#include "logging/log.h"

static int usbkbd_init(drv_manifest_t* driver)
{
    KLOG_DBG("Initializing usbkbd\n");
    return 0;
}

static int usbkbd_exit()
{
    KLOG_DBG("Exiting usbkbd\n");
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
