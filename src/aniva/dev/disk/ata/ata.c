#include "dev/core.h"
#include "dev/manifest.h"
#include "dev/pci/pci.h"
#include <dev/pci/definitions.h>

static pci_dev_id_t ata_dev_ids[] = {
    PCI_DEVID_CLASSES_EX(MASS_STORAGE, PCI_SUBCLASS_ATA, 0, PCI_DEVID_USE_CLASS | PCI_DEVID_USE_SUBCLASS),
    PCI_DEVID_CLASSES_EX(MASS_STORAGE, PCI_SUBCLASS_IDE, 0, PCI_DEVID_USE_CLASS | PCI_DEVID_USE_SUBCLASS),
    PCI_DEVID_END,
};

uintptr_t ata_driver_on_packet(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
    return 0;
}

int ata_probe(pci_device_t* device, pci_driver_t* driver)
{
    logln("Found ATA device!");
    return 0;
}

static pci_driver_t ata_pci_driver = {
    .id_table = ata_dev_ids,
    .f_probe = ata_probe,
    .device_flags = NULL,
};

int ata_driver_init(drv_manifest_t* driver)
{
    register_pci_driver(driver, &ata_pci_driver);
    return 0;
}

int ata_driver_exit()
{
    return 0;
}

const aniva_driver_t g_base_ata_driver = {
    .m_name = "ata",
    .m_type = DT_DISK,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = ata_driver_init,
    .f_exit = ata_driver_exit,
    .f_msg = ata_driver_on_packet,
};
EXPORT_DRIVER_PTR(g_base_ata_driver);
