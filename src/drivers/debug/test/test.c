#include "dev/device.h"
#include "dev/driver.h"
#include "devices/shared.h"
#include "logging/log.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/pci/definitions.h>
#include <dev/pci/pci.h>
#include <libk/stddef.h>

#define DRIVER_NAME "debug"
#define DRIVER_DESCRIPTOR "Perform various driver-related debuging"

#define TEST_DEVICE "testdev"

device_t* device;
driver_t* driver;

int test_init(driver_t* driver);
int test_exit();

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};

EXPORT_DRIVER(extern_test_driver) = {
    .m_name = DRIVER_NAME,
    .m_descriptor = DRIVER_DESCRIPTOR,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .m_type = DT_OTHER,
    .f_init = test_init,
    .f_exit = test_exit,
};

static int _enable_test_device(device_t* device)
{
    KLOG_DBG("Enabling test device\n");
    return 0;
}

static int _disable_test_device(device_t* device)
{
    KLOG_DBG("Disabling test device\n");
    return 0;
}

static int _create_test_device(device_t* device)
{
    KLOG_DBG("Creating test device\n");
    return 0;
}

static int _destroy_test_device(device_t* device)
{
    KLOG_DBG("Destroying test device\n");
    return 0;
}

static device_ctl_node_t generic[] = {
    DEVICE_CTL(DEVICE_CTLC_ENABLE, _enable_test_device, NULL),
    DEVICE_CTL(DEVICE_CTLC_DISABLE, _disable_test_device, NULL),
    DEVICE_CTL(DEVICE_CTLC_CREATE, _create_test_device, NULL),
    DEVICE_CTL(DEVICE_CTLC_DESTROY, _destroy_test_device, NULL),
    DEVICE_CTL_END,
};

/*
 * This driver will be used to test kernel subsystems targeting drivers. Here is a list of things that
 * have been tested:
 *  - PCI device scanning
 * And here is a list of things that are currently being tested:
 *  - Driver-bound devices
 */
int test_init(driver_t* driver)
{
    KLOG_INFO("Initializing test driver!\n");

    /* Regular device */
    device = create_device_ex(driver, "test_device", NULL, DEVICE_CTYPE_SOFTDEV, NULL, generic);

    if (!device)
        return 0;

    device_register(device, NULL);
    return 0;
}

/*!
 * @brief: Perform driver cleanup
 *
 * Make sure we remove the device from this driver on unload
 * NOTE: a device should never be persistant through driver lifetime, so
 * any devices without drivers should be considered fatal errors
 */
int test_exit()
{
    KLOG_INFO("Exiting test driver! =D\n");

    if (device)
        destroy_device(device);
    return 0;
}
