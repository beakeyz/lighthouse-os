
#include "proc/hdrv/driver.h"

static khandle_driver_t device_khandle_driver = {
    .name = "devices",
    .handle_type = HNDL_TYPE_DEVICE,
};

void init_device_khandle_driver()
{
    khandle_driver_register(NULL, &device_khandle_driver);
}
