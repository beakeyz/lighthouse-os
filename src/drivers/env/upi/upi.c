#include "dev/core.h"
#include "dev/driver.h"
#include "dev/manifest.h"
#include "lightos/proc/ipc/pipe/shared.h"

int upi_init(drv_manifest_t* this)
{
    return 0;
}

int upi_exit()
{
    return 0;
}

EXPORT_DRIVER(env_driver) = {
    .m_name = LIGHTOS_UPIPE_DRIVERNAME,
    .m_type = DT_SERVICE,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = upi_init,
    .f_exit = upi_exit,
    .f_msg = nullptr,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};
