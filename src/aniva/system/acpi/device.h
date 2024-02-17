#ifndef __ANIVA_ACPI_DEVICE__
#define __ANIVA_ACPI_DEVICE__

#include "libk/data/linkedlist.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/tables.h"

typedef struct acpi_power_state {
  list_t* resources;
} acpi_power_state_t;

typedef struct acpi_device_power {
  int c_state;
  acpi_power_state_t states[ACPI_D_STATE_COUNT];
} acpi_device_power_t;

/*
 * Structure to interface with the ACPI parts of devices
 *
 * These are created together with normal devices when the acpi namespaces are 
 * enumerated
 */
typedef struct apci_device {
  acpi_handle_t handle;
  acpi_device_power_t pwr;

  const char* acpi_path;
  const char* hid;
} acpi_device_t;

#endif // !__ANIVA_ACPI_DEVICE__
