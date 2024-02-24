#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/device.h"
#include "system/acpi/parser.h"

static kerror_t _acpi_exp_pm_getstate(acpi_device_t* device, int* state)
{
  size_t _state;

  if (ACPI_FAILURE(acpi_eval_int(device->handle, "_PSC", NULL, &_state)))
    return -KERR_INVAL;

  *state = _state;
  return KERR_NONE;
}

static kerror_t _acpi_exp_pm_setstate(acpi_device_t* device, int state)
{
  ACPI_STATUS status;
  char method[5] = { '_', 'P', 'S', '0' + state, '\0' };

  status = AcpiEvaluateObject(device->handle, method, NULL, NULL);
  if (ACPI_FAILURE(status))
      return -KERR_NODEV;

  return 0;
}

kerror_t acpi_device_set_pwrstate(acpi_device_t* device, int state)
{
  kerror_t error;

  error = _acpi_exp_pm_setstate(device, state);

  if (error)
    return error;

  device->pwr.c_state = state;
  return 0;
}

kerror_t acpi_device_get_pwrstate(acpi_device_t* device, int* state)
{
  return _acpi_exp_pm_getstate(device, state);
}
