#include "mem/heap.h"
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acnames.h"
#include "system/acpi/acpica/acnamesp.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/device.h"
#include "system/acpi/tables.h"
#include <dev/core.h>
#include <dev/driver.h>

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

  if (ACPI_FAILURE(AcpiGetHandle(dev, "_HID", &tmp)) || ACPI_FAILURE(AcpiGetHandle(dev, "_ADR", &tmp)))
    return AE_OK;

  /* Get the full path of this device and print it */
  Status = AcpiNsHandleToPathname(dev, &Path, true);

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
 * @brief: Entrypoint of the acpi dev driver
 */
static int _init_acpidev()
{
  ACPI_STATUS stat;
  acpi_handle_t sysbus_hndl;

  stat = AcpiGetHandle(NULL, ACPI_NS_ROOT_PATH, &sysbus_hndl);

  if (!ACPI_SUCCESS(stat))
    return 0;

  stat = AcpiWalkNamespace(ACPI_TYPE_DEVICE, sysbus_hndl, ACPI_UINT32_MAX, register_acpi_device, NULL, NULL, NULL);

  if (!ACPI_SUCCESS(stat))
    return 0;

  return 0;
}

static int _exit_acpidev()
{
  return 0;
}

EXPORT_DRIVER(acpidev) = {
  .m_name = "acpidev",
  .m_descriptor = "Generic ACPI device driver",
  .m_version = DRIVER_VERSION(0, 0, 1),
  .m_type = DT_FIRMWARE,
  .f_init = _init_acpidev,
  .f_exit = _exit_acpidev,
};

EXPORT_DEPENDENCIES(deps) = {
  DRV_DEP_END,
};
