#ifndef __ANIVA_ACPI_DEVICE__
#define __ANIVA_ACPI_DEVICE__

#include "dev/pci/pci.h"
#include "libk/data/linkedlist.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/tables.h"

struct device_endpoint;

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

kerror_t acpi_add_device(acpi_handle_t handle, int type, struct device_endpoint* eps, const char* acpi_path);

kerror_t acpi_device_get_pci_addr(acpi_device_t* device, pci_device_address_t* address);

#endif // !__ANIVA_ACPI_DEVICE__
