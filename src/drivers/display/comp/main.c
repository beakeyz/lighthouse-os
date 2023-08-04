#include <dev/core.h>
#include <dev/driver.h>

int init_window_driver()
{
  return 0;
}

int exit_window_driver()
{
  return 0;
}

uintptr_t msg_window_driver(packet_payload_t payload, packet_response_t** response)
{
  return 0;
}

/*
 * The Light window driver
 *
 * This 'service' will act as generic window manager in other systems, like linux, but we've packed
 * it in a kernel driver. This has a few advantages:
 *  o Better communication with the graphics drivers
 *  o Faster load times
 *  o Safer
 * It also has a few downsides or counterarguments
 *  o No REAL need to be a kernel component
 *  o Harder to customize
 */
EXPORT_DRIVER(window_driver) = {
  .m_name = "lwnd",
  .m_type = DT_SERVICE,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = init_window_driver,
  .f_exit = exit_window_driver,
  .f_drv_msg = msg_window_driver,
  .m_dep_count = 0,
  .m_dependencies = { 0 },
};
