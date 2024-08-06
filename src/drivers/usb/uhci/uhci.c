#include "dev/pci/pci.h"
#include "devices/pci.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <dev/manifest.h>

static pci_dev_id_t uhci_ids[] = {
    PCI_DEVID_CLASSES(SERIAL_BUS_CONTROLLER, PCI_SUBCLASS_SBC_USB, PCI_PROGIF_UHCI),
    PCI_DEVID_END,
};

static int uhci_probe(pci_device_t* device, pci_driver_t* driver)
{
    return 0;
}

static pci_driver_t uhci_pci = {
    .f_probe = uhci_probe,
    .id_table = uhci_ids,
    .device_flags = NULL,
};

static int uhci_init(drv_manifest_t* driver)
{
    register_pci_driver(driver, &uhci_pci);
    return 0;
}

static int uhci_exit(void)
{
    unregister_pci_driver(&uhci_pci);
    return 0;
}

EXPORT_DRIVER(uhci) = {
    .m_name = "uhci",
    .m_descriptor = "UHCI hc driver",
    .m_type = DT_IO,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = uhci_init,
    .f_exit = uhci_exit,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};
