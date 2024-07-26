#include <dev/core.h>
#include <dev/driver.h>

/*!
 * @brief: Initialization code which is ran after dependencies are loaded
 *
 * Scan the USB tree created by the hardware-specific USB hc drivers and try to collect
 * drivers for the devices
 */
static int _usbcore_init()
{
    return 0;
}

static int _usbcore_exit()
{
    return 0;
}

EXPORT_DRIVER(usbcore) = {
    .m_name = "usbcore",
    .m_descriptor = "USB high level core driver",
    .m_type = DT_IO,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = _usbcore_init,
    .f_exit = _usbcore_exit,
};

EXPORT_DEPENDENCIES(deps) = {
    // DRV_DEP(DRV_DEPTYPE_PATH, DRVDEP_FLAG_RELPATH | DRVDEP_FLAG_OPTIONAL, "xhci.drv"),
    DRV_DEP(DRV_DEPTYPE_PATH, DRVDEP_FLAG_RELPATH, "usbkbd.drv"),
    DRV_DEP(DRV_DEPTYPE_PATH, DRVDEP_FLAG_RELPATH | DRVDEP_FLAG_OPTIONAL, "ehci.drv"),
    DRV_DEP_END,
};
