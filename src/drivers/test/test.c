#include <dev/core.h>
#include <dev/driver.h>
#include <libk/stddef.h>

int test_init() {
  return 0;
}

DEFINE_EXPORTED_DRIVER(extern_test_driver) 
  =
{
  .m_name = "ExternalTest",
  .m_descriptor = "Just funnie test",
  .m_version = DRIVER_VERSION(0, 0, 1),
  .m_type = DT_OTHER,
  .f_init = test_init,
  .m_dep_count = 0,
};
