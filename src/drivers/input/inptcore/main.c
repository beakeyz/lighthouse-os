#include "dev/core.h"
#include "dev/group.h"
#include "dev/loader.h"
#include "kevent/event.h"
#include <dev/driver.h>
#include <dev/device.h>

/*
 * Input core driver
 *
 * This driver gets loaded as a part of the hardware spinup procedure (init_hw() in anica/dev/device.c).
 * 
 */

static bool __check_input_device(device_t* device)
{
  /* TODO: Check if we have drivers which can handle the given device */
  return true;
}

/*!
 * @brief: Input core init
 *
 * Walk a few bus devices to see if there are any inputdevices connected.
 * The usb hc driver should have enumerated it's ports and slots and it will identify any
 * input devices which are connected to its bus. 
 */
static int _init_input_core()
{
  dgroup_t* usb_grp;

  if (!KERR_OK(dev_group_get("Dev/usb", &usb_grp)))
    return -1;

  device_for_each(usb_grp, __check_input_device);

  if (kevent_flag_isset(kevent_get("keyboard"), KEVENT_FLAG_FROZEN))
    ASSERT_MSG(load_external_driver("Root/System/i8042.drv"), "Failed to load fallback input driver");

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
  DRV_DEP_END,
};
