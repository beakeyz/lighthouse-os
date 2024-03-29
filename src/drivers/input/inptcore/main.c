#include "dev/core.h"
#include <dev/driver.h>
#include <dev/device.h>

/*
 * Input core driver
 *
 * This driver gets loaded as a part of the hardware spinup procedure (init_hw() in anica/dev/device.c).
 * 
 */

static int _init_input_core()
{
  logln("Initalizing input driver!");
  return 0;
}

static int _exit_input_core()
{
  return 0;
}

/*!
 * @brief: Called when we try to match this driver to a device
 *
 * @returns: 0 when the probe is successfull, otherwise a kerr code
 */
static kerror_t _probe_input_core(aniva_driver_t* driver, device_t* dev)
{
  return -KERR_INVAL;
}

EXPORT_DRIVER(i8042) = {
  .m_name = "inputcore",
  .m_descriptor = "core input driver",
  .m_type = DT_IO,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = _init_input_core,
  .f_exit = _exit_input_core,
  .f_probe = _probe_input_core,
};

EXPORT_DEPENDENCIES(deps) = {
  DRV_DEP(DRV_DEPTYPE_PATH, NULL, "Root/System/i8042.drv"),
  DRV_DEP_END,
};
