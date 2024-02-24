#include <dev/core.h>
#include <dev/driver.h>

static int _gpio_init()
{
  return 0;
}

static int _gpio_exit()
{
  return 0;
}

EXPORT_DRIVER(gpio_driver) = {
  .m_name = "gpio",
  .m_type = DT_IO,
  .m_descriptor = "Access to the GPIO space",
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = _gpio_init,
  .f_exit = _gpio_exit,
};

EXPORT_DEPENDENCIES(deps) = {
  DRV_DEP_END,
};
