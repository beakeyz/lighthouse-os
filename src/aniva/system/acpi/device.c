#include "acpi.h"
#include "acpica/acpi.h"
#include "libk/flow/error.h"
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acnamesp.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/tables.h"

ACPI_STATUS register_acpi_device(acpi_handle_t dev, uint32_t lvl, void* ctx, void** ret)
{
  ACPI_STATUS Status;
  ACPI_DEVICE_INFO* Info;
  ACPI_BUFFER Path;
  char Buffer[256];

  Path.Length = sizeof (Buffer);
  Path.Pointer = Buffer;

  /* Get the full path of this device and print it */
  Status = AcpiNsHandleToPathname (dev, &Path, true);

  if (ACPI_SUCCESS (Status))
  {
    printf ("%s: ", (char*)Path.Pointer);
  }

  Status = AcpiGetObjectInfo(dev, &Info);

  if (ACPI_SUCCESS (Status))
  {
    
    printf (" HID: %s, ADR: %llX, Name: %x\n", Info->HardwareId.String, Info->Address, Info->Name);
  }

  return NULL;
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

  stat = AcpiGetHandle(NULL, "\\_SB", &sysbus_hndl);

  if (!ACPI_SUCCESS(stat))
    return;

  stat = AcpiWalkNamespace(ACPI_TYPE_DEVICE, sysbus_hndl, ACPI_UINT32_MAX, register_acpi_device, NULL, NULL, NULL);

  if (!ACPI_SUCCESS(stat))
    return;

  kernel_panic("TMP: did apci device thing");
}
