#ifndef __ANIVA_DRV_ACPIDEV__
#define __ANIVA_DRV_ACPIDEV__

#include "libk/flow/error.h"
#include "system/acpi/device.h"

extern kerror_t acpi_device_set_pwrstate(acpi_device_t* device, int state);
extern kerror_t acpi_device_get_pwrstate(acpi_device_t* device, int *state);

#endif // !__ANIVA_DRV_ACPIDEV__
