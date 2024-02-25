#include <dev/core.h>
#include <dev/driver.h>

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
  DRV_DEP(DRV_DEPTYPE_PATH, DRVDEP_FLAG_RELPATH, "xhci.drv"),
  DRV_DEP_END,
};
