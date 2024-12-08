#include "ahci_device.h"
#include "dev/core.h"
#include "dev/device.h"
#include "dev/disk/ahci/ahci_port.h"
#include "dev/disk/ahci/definitions.h"
#include "dev/driver.h"
#include "dev/pci/io.h"
#include "dev/pci/pci.h"
#include "lightos/dev/pci.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "sync/mutex.h"
#include <crypto/k_crc32.h>
#include <mem/heap.h>
#include <mem/kmem_manager.h>
#include <oss/obj.h>

static uint32_t _ahci_dev_count;
static ahci_device_t* __ahci_devices = nullptr;
static driver_t* _ahci_driver;

static pci_dev_id_t ahci_id_table[] = {
    PCI_DEVID_CLASSES(MASS_STORAGE, PCI_SUBCLASS_SATA, PCI_PROGIF_SATA),
    PCI_DEVID_END
};

int ahci_driver_init(driver_t* driver);
int ahci_driver_exit();

static ALWAYS_INLINE void* get_hba_region(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS reset_hba(ahci_device_t* device);
static ALWAYS_INLINE ANIVA_STATUS initialize_hba(ahci_device_t* device);

static ALWAYS_INLINE registers_t* ahci_irq_handler(registers_t* regs);

static void __register_ahci_device(ahci_device_t* device)
{
    device->m_idx = _ahci_dev_count++;
    device->m_next = __ahci_devices;
    __ahci_devices = device;
}

static void __unregister_ahci_device(ahci_device_t* device)
{
    ahci_device_t** target;

    for (target = &__ahci_devices; *target; target = &(*target)->m_next) {

        if (*target == device) {
            *target = device->m_next;
            device->m_next = nullptr;

            return;
        }
    }
}

/*!
 * @brief: Register a new AHCI port entry
 */
static inline void _register_ahci_port(ahci_device_t* dev, ahci_port_t* port)
{
    list_append(dev->m_ports, port);
}

uint32_t ahci_mmio_read32(uintptr_t base, uintptr_t offset)
{
    return *(volatile uint32_t*)(base + offset);
}

void ahci_mmio_write32(uintptr_t base, uintptr_t offset, uint32_t data)
{
    volatile uint32_t* current_data = (volatile uint32_t*)(base + offset);
    *current_data = data;
}

static ALWAYS_INLINE void* get_hba_region(ahci_device_t* device)
{
    uintptr_t bar_addr;
    uintptr_t hba_region;

    const uint32_t bus_num = device->m_identifier->address.bus_num;
    const uint32_t device_num = device->m_identifier->address.device_num;
    const uint32_t func_num = device->m_identifier->address.func_num;
    uint32_t bar5;
    g_pci_type1_impl.read32(bus_num, device_num, func_num, BAR5, &bar5);

    /* Grab the bar addr */
    bar_addr = get_bar_address(bar5);

    /* Map the range */
    ASSERT(!__kmem_kernel_alloc((void**)&hba_region, bar_addr, ALIGN_UP(sizeof(HBA), SMALL_PAGE_SIZE * 2), 0, KMEM_FLAG_DMA | KMEM_FLAG_KERNEL));

    return (void*)hba_region;
}

/*!
 * @brief Reset the HBA hardware and scan for ports
 *
 */
static ANIVA_STATUS reset_hba(ahci_device_t* device)
{

    // get this mofo up and running and disable interrupts
    ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC,
        (ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC) | AHCI_GHC_AE) & ~(AHCI_GHC_IE));
    ;

    uint32_t pi = device->m_hba_region->control_regs.ports_impl;

    uintptr_t internal_index = 0;
    for (uint32_t i = 0; i < MAX_HBA_PORT_AMOUNT; i++) {
        if ((pi & (1ULL << i)) != (1ULL << i))
            continue;

        uintptr_t port_offset = 0x100 + i * 0x80;

        volatile HBA_port_registers_t* port_regs = ((volatile HBA_port_registers_t*)&device->m_hba_region->ports[i]);

        uint32_t sig = ahci_mmio_read32((uintptr_t)device->m_hba_region + port_offset, AHCI_REG_PxSIG);

        bool valid = false;

        // TODO: take these sigs out of this
        // function scope and into a macro
        switch (sig) {
        case 0xeb140101:
            KLOG_DBG("AHCI: found ATAPI (CD, DVD)\n");
            break;
        case AHCI_PxSIG_ATA:
            KLOG_DBG("AHCI: found hard drive\n");
            valid = true;
            break;
        case 0xffff0101:
            KLOG_DBG("AHCI: no dev\n");
            break;
        default:
            KLOG_DBG("ACHI: unsupported signature: %d\n", port_regs->signature);
            break;
        }

        if (!valid)
            continue;

        ahci_port_t* port = create_ahci_port(device, (uintptr_t)device->m_hba_region + port_offset, internal_index);
        if (initialize_port(port) == ANIVA_FAIL) {
            KLOG_DBG("AHCI: Failed to initialize port! killing port...\n");
            destroy_ahci_port(port);
            continue;
        }

        /*
         * FIXME: Simplefy these 3 statements below
         */

        _register_ahci_port(device, port);

        internal_index++;
    }

    return ANIVA_SUCCESS;
}

static kerror_t gather_ports_info(ahci_device_t* device)
{

    ANIVA_STATUS status;

    FOREACH(i, device->m_ports)
    {
        ahci_port_t* port = i->data;

        status = ahci_port_gather_info(port);

        if (status == ANIVA_FAIL) {
            KLOG_DBG("AHCI: Failed to gather port info!");
            return -1;
        }
    }

    return (0);
}

static void maybe_claim_hba(ahci_device_t* device)
{
    // We might need to fetch AHCI from the BIOS
    if (ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_CAP2) & AHCI_CAP2_BOH) {
        uint32_t bohc = ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_BOHC) | AHCI_BOHC_OOS;
        ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_BOHC, bohc);

        while (ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_BOHC) & (AHCI_BOHC_BOS | AHCI_BOHC_BB)) {
            udelay(100);
        }
    }
}

static ANIVA_STATUS initialize_hba(ahci_device_t* device)
{
    int error;
    ANIVA_STATUS status;
    uint32_t ghc;

    /* Get the HBA mmio region */
    device->m_hba_region = get_hba_region(__ahci_devices);

    /* Claim the HBA from BIOS if we need to */
    maybe_claim_hba(device);

    /* Reset HBA hardware */
    status = reset_hba(device);

    /* Fuck */
    if (status != ANIVA_SUCCESS)
        return status;

    // HBA has been reset, enable its interrupts and claim this line
    /* TODO: notify PCI of any interrupt line changes */
    error = pci_device_allocate_irq(device->m_identifier, NULL, NULL, ahci_irq_handler, NULL, "AHCI controller");

    /* TODO: handle this propperly =)))) */
    ASSERT_MSG(error == NULL, "Failed to allocate an IRQ for this AHCI device!");

    ghc = ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC) | AHCI_GHC_IE;
    ahci_mmio_write32((uintptr_t)device->m_hba_region, AHCI_REG_AHCI_GHC, ghc);

    KLOG_DBG("AHCI: Gathering info about ports...\n");

    /* Try and send an identify command */
    if (gather_ports_info(device) != 0)
        return ANIVA_FAIL;

    return status;
}

static registers_t* ahci_irq_handler(registers_t* regs)
{

    ahci_device_t* device = __ahci_devices;

    // TODO: handle
    // kernel_panic("Recieved AHCI interrupt!");

    while (device) {

        uint32_t ahci_interrupt_status = 0;

        ahci_mmio_read32((uintptr_t)device->m_hba_region, AHCI_REG_IS);

        if (ahci_interrupt_status == 0)
            goto next;

        FOREACH(i, device->m_ports)
        {
            ahci_port_t* port = i->data;

            /* Sanity check */
            ASSERT(port->m_port_index < 32);

            if (ahci_interrupt_status & (1 << port->m_port_index)) {
                ahci_port_handle_int(port);
            }
        }

    next:
        device = device->m_next;
    }

    return regs;
}

/*
 * @brief: Create a new AHCI device
 *
 * ahci devices are devices for the controllers that we find on the PCI bus.
 * This means that this device needs to be attached to the group of that bus.
 */
ahci_device_t* create_ahci_device(pci_device_t* identifier)
{
    device_t* dev;
    ahci_device_t* ahci_device = kmalloc(sizeof(ahci_device_t));
    char name_buffer[16];

    memset(ahci_device, 0, sizeof(ahci_device_t));
    memset(name_buffer, 0, sizeof(name_buffer));

    ahci_device->m_identifier = identifier;
    ahci_device->m_ports = init_list();
    ahci_device->m_parent = _ahci_driver;

    /* Register ourselves */
    __register_ahci_device(ahci_device);

    /* Format */
    sfmt(name_buffer, "ahci%d", ahci_device->m_idx);

    dev = identifier->dev;

    mutex_lock(dev->lock);

    /* Take the device from PCI */
    (void)driver_takeover_device(_ahci_driver, dev, name_buffer, NULL, ahci_device);

    mutex_unlock(dev->lock);

    if (initialize_hba(ahci_device) == ANIVA_FAIL) {
        destroy_ahci_device(ahci_device);
        return nullptr;
    }

    return ahci_device;
}

void destroy_ahci_device(ahci_device_t* device)
{
    __unregister_ahci_device(device);

    FOREACH(i, device->m_ports)
    destroy_ahci_port(i->data);

    destroy_list(device->m_ports);

    kfree(device);
}

static int ahci_probe(pci_device_t* device, pci_driver_t* driver)
{
    ahci_device_t* ahci_device;

    /* Enable the device first */
    pci_device_enable(device);

    ahci_device = create_ahci_device(device);

    if (!ahci_device)
        return -1;

    return 0;
}

pci_driver_t ahci_pci_driver = {
    .id_table = ahci_id_table,
    .f_probe = ahci_probe,
    .device_flags = NULL,
};

uintptr_t ahci_driver_on_packet(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{

    if (__ahci_devices == nullptr) {
        return (uintptr_t)-1;
    }

    kernel_panic("TODO: implement ahci on_packet functions");

    return 0;
}

/*
 * Drv/disk/ahci
 */
aniva_driver_t base_ahci_driver = {
    .m_name = "ahci",
    .m_type = DT_DISK,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = ahci_driver_init,
    .f_exit = ahci_driver_exit,
    .f_msg = ahci_driver_on_packet,
};
EXPORT_DRIVER_PTR(base_ahci_driver);

/*!
 * @brief: Initialize the AHCI bus+device driver
 *
 * TODO: refactor filenames to something logical lmao
 *       ahci_device -> ahci_bus
 *       ahci_port -> ahci_device (Something like this?)
 *
 * Here we register our PCI driver (So we are in line to recieve the ACHI controllers PCI device once the
 * PCI bus driver is initialized) and we register our own bus group. The group will simply be called 'ahci' which is
 * where all the ahci controllers and devices we find will be attached.
 */
int ahci_driver_init(driver_t* driver)
{
    __ahci_devices = nullptr;
    _ahci_dev_count = NULL;
    _ahci_driver = driver;

    register_pci_driver(driver, &ahci_pci_driver);

    return 0;
}

int ahci_driver_exit()
{
    // shutdown ahci device
    println("Shut down ahci driver");

    unregister_pci_driver(&ahci_pci_driver);

    if (__ahci_devices)
        destroy_ahci_device(__ahci_devices);

    return 0;
}
