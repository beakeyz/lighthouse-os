
#include <dev/core.h>
#include <dev/driver.h>

EXPORT_DRIVER(env_driver) = {
  .m_name = "lenv",
  .m_type = DT_SERVICE,
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = nullptr,
  .f_exit = nullptr,
  .f_msg = nullptr,
  .m_dep_count = 0,
  .m_dependencies = { 0 },
};
