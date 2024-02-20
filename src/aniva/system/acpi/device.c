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

static kerror_t acpi_add_device(acpi_handle_t handle, int type)
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

ACPI_STATUS register_acpi_device(acpi_handle_t dev, uint32_t lvl, void* ctx, void** ret)
{
  kerror_t error;
  ACPI_STATUS Status;
  ACPI_OBJECT_TYPE obj_type;
  ACPI_BUFFER Path;
  acpi_handle_t tmp;
  char Buffer[256];

  Path.Length = sizeof (Buffer);
  Path.Pointer = Buffer;

  if (ACPI_FAILURE(AcpiGetType(dev, &obj_type)))
    return AE_OK;

  if (ACPI_FAILURE(AcpiGetHandle(dev, "_HID", &tmp)))
    return AE_OK;

  /* Get the full path of this device and print it */
  Status = AcpiNsHandleToPathname (dev, &Path, true);

  if (ACPI_SUCCESS (Status))
    printf ("%s: ", (char*)Path.Pointer);

  /* TODO: what to do on different types? */
  switch (obj_type) {
    case ACPI_TYPE_DEVICE:
    case ACPI_TYPE_ANY:
      error = acpi_add_device(dev, obj_type);
      /* FIXME: Ignore these failures? */
      if (error)
        return AE_OK;
      break;
    default:
      break;
  }

  return AE_OK;
}

/*!
 * @brief: Initialize typical ACPI devices
 * 
 * Called after ACPICA is initialized
 * We loop over every acpi device on the system bus and register it for our
 * driver framework to initialize them later
 */
void init_acpi_devices()
{
  ACPI_STATUS stat;
  acpi_handle_t sysbus_hndl;

  _acpi_device_list = init_list();

  stat = AcpiGetHandle(NULL, ACPI_NS_ROOT_PATH, &sysbus_hndl);

  if (!ACPI_SUCCESS(stat))
    return;

  stat = AcpiWalkNamespace(ACPI_TYPE_DEVICE, sysbus_hndl, ACPI_UINT32_MAX, register_acpi_device, NULL, NULL, NULL);

  if (!ACPI_SUCCESS(stat))
    return;

}
