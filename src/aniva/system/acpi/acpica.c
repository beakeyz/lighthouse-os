#include "acpi.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
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

ACPI_STATUS init_acpi_early()
{
    ACPI_STATUS stat;

    stat = AcpiInitializeSubsystem();

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to initialize ACPI subsystem\n");
        /* TODO: again, let the system know */
        return stat;
    }

    stat = AcpiInitializeTables(NULL, 16, true);

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to initailize ACPI tables\n");
        return stat;
    }

    stat = AcpiLoadTables();

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to load ACPI tables\n");
        return stat;
    }

    return 0;
}

ACPI_STATUS init_acpi_full()
{
    ACPI_OBJECT arg;
    ACPI_OBJECT_LIST param;
    ACPI_STATUS stat;

    /* Initialize system interrupt controller (1 = APIC, 0 = PIC) */
    arg.Integer.Type = ACPI_TYPE_INTEGER;
    arg.Integer.Value = g_system_info.irq_chip_type;

    param.Count = 1;
    param.Pointer = &arg;

    /* Ignore status */
    AcpiEvaluateObject(NULL, (ACPI_STRING) "\\_PIC", &param, NULL);

    stat = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to enable ACPI\n");
        return stat;
    }

    stat = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to initialize ACPI objects\n");
        return stat;
    }

    stat = AcpiInstallGlobalEventHandler(generic_global_event_handler, NULL);

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to install ACPI global event handler\n");
        return stat;
    }

    stat = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_ALL_NOTIFY, generic_glob_notif_handler, NULL);

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to install ACPI global notifier\n");
        return stat;
    }

    stat = AcpiEnableAllRuntimeGpes();

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to enable ACPI runtime GPEs\n");
        return stat;
    }

    stat = AcpiUpdateAllGpes();

    if (ACPI_FAILURE(stat)) {
        KLOG_ERR("Failed to update ACPI runtime GPEs\n");
        return stat;
    }

    return AE_OK;
}
