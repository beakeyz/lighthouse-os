#include <dev/core.h>
#include <dev/driver.h>

static int _init_acpicore()
{
  return 0;
}

static int _exit_acpicore()
{
  return 0;
}

EXPORT_DRIVER(acpicore) = {
  .m_name = "acpicore",
  .m_descriptor = "High level ACPI driver",
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = _init_acpicore,
  .f_exit = _exit_acpicore,
};

EXPORT_DEPENDENCIES(deps) = {
  //DRV_DEP(DRV_DEPTYPE_PATH, DRVDEP_FLAG_RELPATH, "acpi")
  DRV_DEP_END,
};
