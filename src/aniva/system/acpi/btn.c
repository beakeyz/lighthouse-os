
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/acpica/actypes.h"

/*
 * ACPI button devices
 *
 * Here we handle button presses through ACPI.
 */

struct acpi_btn {
  
};

uint32_t _acpi_handle_btn_press(void* _btn)
{
  return ACPI_INTERRUPT_HANDLED;
}

void _init_acpi_btns()
{
  AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON, _acpi_handle_btn_press, NULL);
}
