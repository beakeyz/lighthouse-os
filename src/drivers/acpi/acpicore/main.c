#include <dev/core.h>
#include <dev/driver.h>

/*!
 * @brief: Entrypoint of the acpi core driver
 */
static int _init_acpidev()
{
    return 0;
}

static int _exit_acpidev()
{
    return 0;
}

EXPORT_DRIVER(acpidev) = {
    .m_name = "acpicore",
    .m_descriptor = "High level ACPI driver",
    .m_version = DRIVER_VERSION(0, 0, 1),
    .m_type = DT_FIRMWARE,
    .f_init = _init_acpidev,
    .f_exit = _exit_acpidev,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP(DRV_DEPTYPE_PATH, DRVDEP_FLAG_RELPATH, "acpidev.drv"),
    DRV_DEP_END,
};
