#include "dev/device.h"
#include "dev/endpoint.h"
#include "libk/flow/error.h"
#include "system/acpi/acpi.h"
#include "system/acpi/acpica/acexcep.h"
#include "system/acpi/acpica/acnames.h"
#include "system/acpi/acpica/acnamesp.h"
#include "system/acpi/acpica/acpixf.h"
#include "system/acpi/acpica/actbl3.h"
#include "system/acpi/acpica/actypes.h"
#include "system/acpi/device.h"
#include "system/acpi/parser.h"
#include "system/acpi/tables.h"
#include <dev/core.h>
#include <dev/driver.h>

#include "acpidev.h"

struct {
    uintptr_t addr;
    uint32_t interrupt;
    uint16_t pci_deviceid;
    uint16_t pci_vendorid;
} spcr_uart;

static uint64_t _acpi_dev_msg(device_t* device, dcc_t code)
{
    return KERR_NONE;
}

static int _acpi_dev_create(device_t* device)
{
    return 0;
}

struct device_generic_endpoint _acpi_generic_dep = {
    .f_create = _acpi_dev_create,
    .f_msg = _acpi_dev_msg,
    NULL,
};

/*!
 * @brief: ...
 *
 * TODO: how the fuck do we do a device power-on through ACPI?
 */
static kerror_t _acpi_dev_power_on(device_t* device)
{
    acpi_device_t* acpi_dev;

    if (!device || !device->private)
        return -KERR_INVAL;

    acpi_dev = device->private;

    return acpi_device_set_pwrstate(acpi_dev, ACPI_STATE_D0);
}

struct device_pwm_endpoint _acpi_pwm_dep = {
    .f_power_on = _acpi_dev_power_on,
};

/*
 * Endpoint table for the acpi device driver
 *
 * Here we define the endpoints our devices adhere to. These will mostly be power management
 * opperations, since that is kinda what ACPI is meant for lmao
 */
static device_ep_t _acpi_eps[] = {
    { ENDPOINT_TYPE_GENERIC, sizeof(struct device_generic_endpoint), { &_acpi_generic_dep } },
    { ENDPOINT_TYPE_PWM, sizeof(struct device_pwm_endpoint), {
                                                                 &_acpi_pwm_dep,
                                                             } },
    {
        NULL,
    },
};

static bool _should_ignore_device(acpi_handle_t dev)
{
    // TODO:
    return false;
}

ACPI_STATUS register_acpi_device(acpi_handle_t dev, uint32_t lvl, void* ctx, void** ret)
{
    kerror_t error;
    ACPI_STATUS Status;
    ACPI_OBJECT_TYPE obj_type;
    ACPI_BUFFER Path;
    char Buffer[256];

    Path.Length = sizeof(Buffer);
    Path.Pointer = Buffer;

    if (ACPI_FAILURE(AcpiGetType(dev, &obj_type)))
        return AE_OK;

    /* Get the full path of this device and print it */
    Status = AcpiNsHandleToPathname(dev, &Path, true);

    if (!ACPI_SUCCESS(Status))
        return AE_OK;

    /* TODO: what to do on different types? */
    switch (obj_type) {
    case ACPI_TYPE_DEVICE:
        if (_should_ignore_device(dev))
            return AE_OK;
        break;
    default:
        break;
    }

    error = acpi_add_device(dev, obj_type, _acpi_eps, Path.Pointer);
    /* FIXME: Ignore these failures? */
    if (error)
        return AE_OK;

    return AE_OK;
}

static void _acpidev_do_uart_ignore()
{
    ACPI_STATUS status;
    struct acpi_table_stao* stao_tbl;
    struct acpi_table_spcr* spcr_tbl;

    stao_tbl = NULL;
    spcr_tbl = NULL;

    status = AcpiGetTable(ACPI_SIG_STAO, NULL, (struct acpi_table_header**)&stao_tbl);

    /* No STAO table, OK */
    if (ACPI_FAILURE(status) || !stao_tbl)
        return;

    if (!stao_tbl->IgnoreUart)
        goto release_table_and_exit;

    status = AcpiGetTable(ACPI_SIG_SPCR, NULL, (struct acpi_table_header**)&spcr_tbl);

    /* No SPRC table, OK */
    if (ACPI_FAILURE(status) || !spcr_tbl)
        return;

    spcr_uart.addr = spcr_tbl->SerialPort.Address;
    spcr_uart.interrupt = spcr_tbl->Interrupt;
    spcr_uart.pci_deviceid = spcr_tbl->PciDeviceId;
    spcr_uart.pci_vendorid = spcr_tbl->PciVendorId;

    AcpiPutTable((struct acpi_table_header*)spcr_tbl);
release_table_and_exit:
    AcpiPutTable((struct acpi_table_header*)stao_tbl);
}

/*!
 * @brief: Entrypoint of the acpi dev driver
 */
static int _init_acpidev()
{
    ACPI_STATUS stat;
    acpi_handle_t sysbus_hndl;

    _acpidev_do_uart_ignore();

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
