#include "dev/core.h"
#include <dev/driver.h>
#include <dev/device.h>

static int _init_i8042()
{
  logln("Initalizing i8042 driver!");
  return 0;
}

static int _exit_i8042()
{
  return 0;
}

/*!
 * @brief: Called when we try to match this driver to a device
 *
 * @returns: 0 when the probe is successfull, otherwise a kerr code
 */
static kerror_t _probe_i8042(aniva_driver_t* driver, device_t* dev)
{
  return -KERR_INVAL;
}

EXPORT_DRIVER(i8042) = {
  .m_name = "i8042",
  .m_descriptor = "i8042 input driver",
  .m_type = DT_IO,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = _init_i8042,
  .f_exit = _exit_i8042,
  .f_probe = _probe_i8042,
};

EXPORT_DEPENDENCIES(deps) = {
  //DRV_DEP(DRV_DEPTYPE_PATH, NULL, "Root/System/"),
  DRV_DEP_END,
};
