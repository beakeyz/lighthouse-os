#include "acpi.h"
#include "acpica/acpi.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include <mem/heap.h>
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/aclocal.h"
#include "system/acpi/acpica/acnamesp.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/device.h"
#include "system/acpi/tables.h"

static list_t* _acpi_device_list;

kerror_t acpi_add_device(acpi_handle_t handle, int type)
{
  acpi_device_t* device;
  ACPI_DEVICE_INFO* info = nullptr;

  device = kmalloc(sizeof(*device));
  if (!device)
    return -KERR_NOMEM;

  AcpiGetObjectInfo(handle, &info);
  if (!info) {
    printf("Failed to get object info\n");
    return AE_OK;
  }

  printf (" HID: %s, ADR: %llX, Name: %x\n", info->HardwareId.String, info->Address, info->Name);

  list_append(_acpi_device_list, device);
  return KERR_NONE;
}

void init_acpi_devices()
{
  _acpi_device_list = init_list();
}
