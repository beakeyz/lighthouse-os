#include "dev/driver.h"
#include <dev/core.h>

int test_driver_init()
{
  return 0;
}

aniva_driver_t test_driver = {
  .m_name = "test_driver",
  .m_flags = DRV_SOCK | DRV_NON_ESSENTIAL,
  .f_init = test_driver_init,
};

EXPORT_DRIVER(test_driver);
