#include "acpi.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/acpica/actypes.h"

static void generic_global_event_handler(UINT32 eventType, ACPI_HANDLE device, UINT32 eventNumber, void* context)
{
  kernel_panic("TODO: handle global events");
}

static void generic_glob_notif_handler(ACPI_HANDLE dev, UINT32 value, void* ctx)
{
  kernel_panic("TODO: handle global notifications");
}

void init_acpi_early()
{
  ACPI_OBJECT arg;
  ACPI_OBJECT_LIST param;
  ACPI_STATUS stat;

  println("A");

  stat = AcpiInitializeSubsystem();

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to initialize ACPI subsystem\n");
    /* TODO: again, let the system know */
    return;
  }

  println("B");

  stat = AcpiInitializeTables(NULL, 16, true);

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to initailize ACPI tables\n");
    return;
  }

  println("C");

  stat = AcpiLoadTables();

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to load ACPI tables\n");
    return;
  }

  println("D");

  /* Initialize system interrupt controller (1 = APIC, 0 = PIC) */
  arg.Integer.Type = ACPI_TYPE_INTEGER;
  arg.Integer.Value = 0;

  param.Count = 1;
  param.Pointer = &arg;

  /* Ignore status */
  AcpiEvaluateObject(NULL, (ACPI_STRING)"\\_PIC", &param, NULL);

  println("E");

  stat = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to enable ACPI\n");
    return;
  }

  println("F");
  
  stat = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to initialize ACPI objects\n");
    return;
  }

  println("G");

  stat = AcpiInstallGlobalEventHandler(generic_global_event_handler, NULL);

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to install ACPI global event handler\n");
    return;
  }

  println("H");

  stat = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_ALL_NOTIFY, generic_glob_notif_handler, NULL);

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to install ACPI global notifier\n");
    return;
  }

  println("I");

  stat = AcpiEnableAllRuntimeGpes();

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to enable ACPI runtime GPEs\n");
    return;
  }

  println("J");

  stat = AcpiUpdateAllGpes();

  if (ACPI_FAILURE(stat))
  {
    printf("Failed to update ACPI runtime GPEs\n");
    return;
  }

  printf("Acpica done!\n");
}
