/*
 * The pci driver for the xHCI host controller
 */

#include "dev/core.h"
#include "dev/driver.h"
#include "dev/driver.h"
#include "dev/usb/hcd.h"
#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
#include <dev/pci/definitions.h>
#include <dev/pci/pci.h>
#include <dev/usb/port.h>

#include "extended.h"
#include "sched/scheduler.h"
#include "xhci.h"

int xhci_init(driver_t* driver);
int xhci_exit();
uintptr_t xhci_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);

static driver_t* _xhci_driver;
pci_dev_id_t xhci_pci_ids[] = {
    PCI_DEVID_CLASSES(SERIAL_BUS_CONTROLLER, PCI_SUBCLASS_SBC_USB, PCI_PROGIF_XHCI),
    PCI_DEVID_END,
};

/*!
 * @brief: Ring the doorbell of a particular slot, endpoint and stream
 *
 * In our case, streamid is most likely going to be 0
 */
static void xhci_ring(xhci_hcd_t* hcd, uint32_t slot, uint32_t endpoint, uint32_t streamid)
{
    uint32_t* db_addr;

    if (slot > hcd->max_slots)
        return;

    if (endpoint >= XHCI_MAX_EPS)
        return;

    db_addr = &hcd->db_arr->db[slot];

    /* Ding, dong */
    mmio_write_dword(db_addr, DB_VALUE(endpoint, streamid));

    /* Flush that shit */
    mmio_read_dword(db_addr);
}

static uint64_t xhci_read64(usb_hcd_t* hub, uintptr_t offset)
{
    return *(volatile uint64_t*)(hcd_to_xhci(hub)->register_ptr + offset);
}

static void xhci_write64(usb_hcd_t* hub, uintptr_t offset, uint64_t value)
{
    *(volatile uint64_t*)(hcd_to_xhci(hub)->register_ptr + offset) = value;
}

static uint32_t xhci_read32(usb_hcd_t* hub, uintptr_t offset)
{
    return *(volatile uint32_t*)(hcd_to_xhci(hub)->register_ptr + offset);
}

static void xhci_write32(usb_hcd_t* hub, uintptr_t offset, uint32_t value)
{
    *(volatile uint32_t*)(hcd_to_xhci(hub)->register_ptr + offset) = value;
}

static uint16_t xhci_read16(usb_hcd_t* hub, uintptr_t offset)
{
    return *(volatile uint16_t*)(hcd_to_xhci(hub)->register_ptr + offset);
}

static void xhci_write16(usb_hcd_t* hub, uintptr_t offset, uint16_t value)
{
    *(volatile uint16_t*)(hcd_to_xhci(hub)->register_ptr + offset) = value;
}

static uint8_t xhci_read8(usb_hcd_t* hub, uintptr_t offset)
{
    return *(volatile uint8_t*)(hcd_to_xhci(hub)->register_ptr + offset);
}

static void xhci_write8(usb_hcd_t* hub, uintptr_t offset, uint8_t value)
{
    *(volatile uint8_t*)(hcd_to_xhci(hub)->register_ptr + offset) = value;
}

/*!
 * @brief Read a certain bit (sequence) in a field until we time out or read the correct value
 *
 * @returns: 0 on success, -1 on failure
 */
static int xhci_wait_read(struct usb_hcd* hub, uintptr_t max_timeout, void* address, uintptr_t mask, uintptr_t expect)
{
    uint32_t value;

    while (max_timeout) {
        value = mmio_read_dword(address);

        if ((value & mask) == expect)
            return 0;

        udelay(1);
        max_timeout--;
    }

    return -1;
}

static kerror_t xhci_submit_cmd(xhci_hcd_t* xhci, uint64_t trb_addr, uint32_t trb_status, uint32_t trb_control)
{
    xhci_ring_t* ring;

    ring = xhci->cmd_ring_ptr;

    if (!ring)
        return -KERR_INVAL;

    xhci_trb_t trb = {
        .addr = trb_addr,
        .status = trb_status,
        .control = trb_control
    };

    /* Put the shit in the ring */
    xhci_cmd_ring_enqueue(xhci, &trb);

    /* boob */
    xhci_ring(xhci, 0, 0, 0);
    return 0;
}

/*!
 * @brief: Set a ports power status
 */
static void xhci_port_power(xhci_hcd_t* hcd, xhci_port_t* port, bool status)
{
    uint32_t current_stat;

    if (!port || !port->base_addr)
        return;

    current_stat = mmio_read_dword(port->base_addr);

    /* Port not enabled */
    if ((current_stat & XHCI_PORT_CCS) != XHCI_PORT_CCS)
        return;

    /* Reserve RO and status bits */
    current_stat = (current_stat & XHCI_PORT_RO) | (current_stat & XHCI_PORT_RWS);

    if (status) {
        mmio_write_dword(port->base_addr, current_stat | XHCI_PORT_POWR);
        mmio_read_dword(port->base_addr);
    } else {
        mmio_write_dword(port->base_addr, current_stat & ~(XHCI_PORT_POWR));
    }
}

int xhci_get_port_sts(struct xhci_hcd* xhci, u8 idx, usb_port_status_t* status)
{
    xhci_hub_t* rhub;
    xhci_port_t* xhci_port;
    u32 port_status;

    if (!xhci || !status)
        return -1;

    rhub = xhci->rhub;

    if (idx >= rhub->port_count)
        return -1;

    /* Grab the xhci port */
    xhci_port = xhci->rhub->ports[idx];

    /* Could we find the port? */
    if (!xhci_port)
        return -KERR_NOT_FOUND;

    /* Read */
    port_status = mmio_read_dword(&xhci->op_regs->ports[idx].port_status_base);

    KLOG_DBG("XHCI: Getting port status for port %d (status=0x%d)\n", idx, port_status);

    /* Grab the port speed */
    u8 port_speed = (port_status >> XHCI_PORT_SPEED_OFFSET) & XHCI_PORT_SPEED_MASK;

    /* Detect hi-lo speed ports */
    if (port_speed == 3)
        status->status |= USB_PORT_STATUS_HIGH_SPEED;
    else if (port_speed == 2)
        status->status |= USB_PORT_STATUS_LOW_SPEED;

    /* Scan regular port status bits */
    if (port_status & XHCI_PORT_CCS)
        status->status |= USB_PORT_STATUS_CONNECTION;
    if (port_status & XHCI_PORT_PED)
        status->status |= USB_PORT_STATUS_ENABLE;
    if (port_status & XHCI_PORT_POWR)
        status->status |= USB_PORT_STATUS_POWER;
    if (port_status & XHCI_PORT_RESET)
        status->status |= USB_PORT_STATUS_RESET;
    if (port_status & XHCI_PORT_OCA)
        status->status |= USB_PORT_STATUS_OVER_CURRENT;

    /* Scan port status change bits */
    if (port_status & XHCI_PORT_CSC)
        status->change |= USB_PORT_STATUS_CONNECTION;
    if (port_status & XHCI_PORT_PEC)
        status->change |= USB_PORT_STATUS_POWER;
    if (port_status & XHCI_PORT_OCC)
        status->change |= USB_PORT_STATUS_OVER_CURRENT;
    if (port_status & XHCI_PORT_RC)
        status->change |= USB_PORT_STATUS_RESET;

    return 0;
}

int xhci_clear_port_ftr(struct xhci_hcd* xhci, u8 idx, u32 feature)
{
    xhci_hub_t* rhub;
    u32 port_status;
    u32* p_port_status;

    if (!xhci)
        return -1;

    rhub = xhci->rhub;

    if (idx >= rhub->port_count)
        return -1;

    /* Compute the port status address */
    p_port_status = &xhci->op_regs->ports[idx].port_status_base;

    /* Read */
    port_status = mmio_read_dword(p_port_status) & ~XHCI_PORT_STATUS_MASK;

    switch (feature) {
    case USB_FEATURE_PORT_ENABLE:
        port_status |= XHCI_PORT_PED;
        break;
    case USB_FEATURE_PORT_SUSPEND:
        return -1;
    case USB_FEATURE_PORT_POWER:
        port_status &= ~XHCI_PORT_POWR;
        break;
    case USB_FEATURE_C_PORT_CONNECTION:
        port_status |= XHCI_PORT_CSC;
        break;
    case USB_FEATURE_C_PORT_OVER_CURRENT:
        port_status |= XHCI_PORT_OCC;
        break;
    case USB_FEATURE_C_PORT_RESET:
        port_status |= XHCI_PORT_RC;
        break;
    default:
        return -KERR_NOT_FOUND;
    }

    /* Write the change */
    mmio_write_dword(p_port_status, port_status);
    /* Flush read the register */
    (void)mmio_read_dword(p_port_status);
    return 0;
}

int xhci_set_port_ftr(struct xhci_hcd* xhci, u8 idx, u32 feature)
{
    xhci_hub_t* rhub;
    u32 port_status;
    u32* p_port_status;

    if (!xhci)
        return -1;

    rhub = xhci->rhub;

    if (idx >= rhub->port_count)
        return -1;

    /* Compute the port status address */
    p_port_status = &xhci->op_regs->ports[idx].port_status_base;

    /* Read */
    port_status = mmio_read_dword(p_port_status) & ~XHCI_PORT_STATUS_MASK;

    switch (feature) {
    case USB_FEATURE_PORT_RESET:
        port_status |= XHCI_PORT_RESET;
        break;
    case USB_FEATURE_PORT_POWER:
        port_status |= XHCI_PORT_POWR;
        break;
    default:
        return -KERR_NOT_FOUND;
    }

    /* Write the change */
    mmio_write_dword(p_port_status, port_status);
    /* Flush read the register */
    (void)mmio_read_dword(p_port_status);
    return 0;
}

/*!
 * @brief Create a xhci (root) hub
 *
 * TODO: The goals for this function are:
 *  - to be able to recieve device_descriptors from every port (device) that is attached to this hub and print out the data we recieved
 *  - to power on the ports we find
 */
int create_xhci_hub(xhci_hub_t** phub, struct xhci_hcd* xhci, struct usb_device* udev)
{
    uint32_t eec;
    uint32_t eecp;
    uint32_t hcc_params;
    xhci_hub_t* hub;

    if (!phub)
        return -KERR_INVAL;

    hub = kzalloc(sizeof(*hub));

    if (!hub)
        return -KERR_INVAL;

    memset(hub, 0, sizeof(*hub));

    hub->port_count = 0;
    hub->port_arr_size = sizeof(xhci_port_t*) * xhci->max_ports;
    hub->ports = kmalloc(hub->port_arr_size);

    if (!hub->ports) {
        kzfree(hub, sizeof(*hub));
        return -KERR_INVAL;
    }

    /* Export the hub lol */
    *phub = hub;

    /* This creates a generic USB hub and asks the host controller for its data */
    if (create_usb_hub(&hub->phub, xhci->parent, USB_HUB_TYPE_XHCI, nullptr, udev, xhci->max_ports, false)) {
        kfree(hub->ports);
        kzfree(hub, sizeof(*hub));
        return -KERR_INVAL;
    }

    memset(hub->ports, 0, hub->port_arr_size);

    /*
     * TODO: send identification packets to the devices on this hub
     * and also send setaddress packets to make sure we know where to access a particular device
     * when we've retrieved the device info, create a device on the usb core driver and
     * try to find a class driver for it. This means we can both have a driver that's already loaded,
     * which we will use to driver this device, OR we need to load a driver from disk somewhere.
     * In that case we'll need to access the base profile variables to see where the driver of
     * a particular USB class is located
     */

    /* Gather port protocols */
    eec = 0xFFFFFFFF;
    hcc_params = mmio_read_dword(&xhci->cap_regs->hcc_params_1);
    eecp = XHCI_HCC_EXT_CAPS(hcc_params) << 2;

    for (; eecp && XHCI_EXT_CAP_NEXT(eec) && hub->port_count <= xhci->max_ports; eecp += (XHCI_EXT_CAP_NEXT(eec) << 2)) {
        eec = xhci_cap_read(xhci, eecp);

        if (XHCI_EXT_CAP_GETID(eec) != XHCI_EXT_CAPS_PROTOCOLS)
            continue;

        if (XHCI_EXT_CAPS_PROTOCOLS_0_MAJOR(eec) > 3)
            continue;

        /* Get this protocol info */
        uint32_t temp = xhci_cap_read(xhci, eecp + 8);
        uint32_t offset = XHCI_EXT_CAPS_PROTOCOLS_1_OFFSET(temp);
        uint32_t count = XHCI_EXT_CAPS_PROTOCOLS_1_COUNT(temp);

        if (!offset || !count)
            continue;

        for (uint32_t i = offset - 1; i < offset + count - 1; i++) {
            kdbgf("speed for port %d is %s\n", i,
                XHCI_EXT_CAPS_PROTOCOLS_0_MAJOR(eec) == 0x3 ? "super" : "high");

            enum USB_SPEED speed = XHCI_EXT_CAPS_PROTOCOLS_0_MAJOR(eec) == 0x3 ? USB_SUPERSPEED : USB_HIGHSPEED;

            if (hub->ports[i])
                hub->ports[i]->speed = speed;
            else
                hub->ports[i] = create_xhci_port(&hub->phub->ports[i], hub, speed);

            hub->port_count++;
        }
    }

    /* Enumerate the standard USB hub */
    usb_hub_enumerate(hub->phub);

    return 0;
}

/*!
 * @brief Deallocate a xhci (root) hub
 *
 */
void destroy_xhci_hub(xhci_hub_t* hub)
{
    xhci_port_t* c_port;

    kernel_panic("TODO: destroy_xhci_hub");

    for (uint32_t i = 0; i < hub->port_count; i++) {
        c_port = hub->ports[i];

        if (!c_port)
            continue;

        /* TODO; */
        // destroy_xhci_port(c_port);
    }

    kfree(hub->ports);
}

/*!
 * @brief: Queue the correct TRBs for this request
 *
 * Since USB uses a few different types of packets, we need to construct different TRB structures
 * based on the type of transfer. Next to that the event thread and transfer finish thread of this
 * hcd need to know how to correctly handle events and transfer finishes
 */
int xhci_enq_request(usb_hcd_t* hcd, usb_xfer_t* req)
{
    xhci_hcd_t* xhci;

    if (!hcd)
        return -1;

    xhci = hcd->private;

    /* Need the xhci hcd */
    if (!xhci)
        return -1;

    /* Need the xhci roothub xD */
    if (!xhci->rhub || !xhci->rhub->phub || !xhci->rhub->phub->udev)
        return -1;

    /* Check if this is a request for our own roothub */
    if (req->req_devaddr == xhci->rhub->phub->udev->dev_addr)
        return xhci_process_rhub_xfer(xhci->rhub->phub, req);

    kernel_panic("TODO: implement xhci_enq_request");

    xhci_ring(xhci, 0, 0, 0);
    return 0;
}

int xhci_deq_request(usb_hcd_t* hcd, usb_xfer_t* req)
{
    kernel_panic("TODO: implement xhci_deq_request");
    return 0;
}

usb_hcd_io_ops_t xhci_io_ops = {
    .enq_request = xhci_enq_request,
    .deq_request = xhci_deq_request,
};

/*!
 * @brief Create and populate scratchpad buffers
 *
 * Allocates the memory needed for scratchpads and puts the dma address in the first scratchpad thing in the device context array
 * requires the dca to be allocated
 */
static int xhci_create_scratchpad(xhci_hcd_t* xhci)
{
    xhci_scratchpad_t* sp;

    /* Already set =/ */
    if (xhci->scratchpad_ptr)
        return -1;

    /* No scratchpad =( */
    if (!xhci->scratchpad_count)
        return 0;

    sp = kmalloc(sizeof(*xhci->scratchpad_ptr));

    if (!sp)
        return -1;

    memset(sp, 0, sizeof(xhci_scratchpad_t));

    /* Allocate the dma arrray */
    ASSERT(!__kmem_kernel_alloc_range(
        (void**)&sp->array,
        sizeof(uintptr_t) * xhci->scratchpad_count, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));

    /* Set the dma (aka physical) address */
    sp->dma = kmem_to_phys(nullptr, (uintptr_t)sp->array);

    /* Allocate the buffers (aka kernel addresses to the scratchpad buffers) */
    sp->buffers = kmalloc(sizeof(void*) * xhci->scratchpad_count);

    /* Allocate all the buffers */
    for (uintptr_t i = 0; i < xhci->scratchpad_count; i++) {
        vaddr_t buffer_addr;
        paddr_t dma_addr;

        /* Allocate the buffer address for the scratchpad */
        ASSERT(!__kmem_kernel_alloc_range((void**)&buffer_addr, SMALL_PAGE_SIZE, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));

        /* Get it's DMA address */
        dma_addr = kmem_to_phys(nullptr, buffer_addr);

        sp->array[i] = dma_addr;
        sp->buffers[i] = (void*)buffer_addr;
    }

    xhci->scratchpad_ptr = sp;

    xhci->dctx_array_ptr->dev_ctx_ptrs[0] = sp->dma;
    return 0;
}

static void xhci_destroy_scratchpad(xhci_hcd_t* xhci)
{
    xhci_scratchpad_t* scratchpad;

    if (!xhci->scratchpad_ptr)
        return;

    scratchpad = xhci->scratchpad_ptr;

    /* Deallocate the buffers */
    for (uintptr_t i = 0; i < xhci->scratchpad_count; i++) {
        __kmem_kernel_dealloc((vaddr_t)scratchpad->buffers[i], SMALL_PAGE_SIZE);
    }

    /* Deallocate the dma array */
    __kmem_kernel_dealloc((vaddr_t)scratchpad->array, sizeof(uintptr_t) * xhci->scratchpad_count);

    /* Free the buffers buffer */
    kfree(scratchpad->buffers);

    /* Lastly, free the scratchpad struct */
    kfree(xhci->scratchpad_ptr);

    /* Make sure the xhci structure knows this is gone */
    xhci->scratchpad_ptr = nullptr;

    /* Also make sure xhci knows its gone */
    if (xhci->dctx_array_ptr) {
        xhci->dctx_array_ptr->dev_ctx_ptrs[0] = NULL;
    }
}

/*!
 * @brief Allocate (DMA) buffers that we and the xhci controller needs
 *
 * Things we need to do:
 * - Program the max slots
 * - Find out the page size (prob just 4Kib)
 * - Setup port array
 *
 * Things we need to allocate:
 * - Device context array
 * - Scratchpads
 * - Command rings
 */
static int xhci_prepare_memory(usb_hcd_t* hcd)
{
    int error;
    uint32_t hcs_params_1;
    uint32_t hcs_params_2;
    xhci_hcd_t* xhci = hcd->private;

    hcs_params_1 = mmio_read_dword(&xhci->cap_regs->hcs_params_1);
    hcs_params_2 = mmio_read_dword(&xhci->cap_regs->hcs_params_2);

    xhci->max_ports = HC_MAX_PORTS(hcs_params_1);
    xhci->max_slots = HC_MAX_SLOTS(hcs_params_1);

    if (!xhci->max_ports)
        return -1;

    /* Program the max slots */
    mmio_write_dword(&xhci->op_regs->config_reg, xhci->max_slots);

    /* Allocate the dctx array */
    ASSERT(!__kmem_kernel_alloc_range((void**)&xhci->dctx_array_ptr, sizeof(xhci_dev_ctx_array_t), NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA | KMEM_FLAG_WRITETHROUGH));
    xhci->dctx_array_ptr->dma = kmem_to_phys(nullptr, (vaddr_t)xhci->dctx_array_ptr);

    /* Clear the dctx array */
    memset(xhci->dctx_array_ptr, 0, sizeof(xhci_dev_ctx_array_t));

    /* Set the dcbaa pointer in the HC */
    mmio_write_qword(&xhci->op_regs->dcbaa_ptr, xhci->dctx_array_ptr->dma);

    /* Allocate a zone allocator for device context structures */
    xhci->device_ctx_allocator = create_zone_allocator(2 * Mib, sizeof(xhci_device_ctx_t), KMEM_FLAG_DMA | KMEM_FLAG_WRITETHROUGH | KMEM_FLAG_KERNEL);

    /* Grab the scratchpad count */
    xhci->scratchpad_count = HC_MAX_SCRTCHPD(hcs_params_2);

    /* NOTE: it is legal for there to be no scratchpad */
    if (xhci->scratchpad_count && xhci_create_scratchpad(xhci))
        return -1;

    /* Set the command ring */
    xhci->cmd_ring_ptr = create_xhci_ring(xhci, XHCI_MAX_COMMANDS, XHCI_RING_TYPE_CMD);

    error = xhci_set_cmd_ring(xhci);

    if (error)
        return -2;

    xhci->interrupter = create_xhci_interrupter(xhci);

    if (!xhci->interrupter)
        return -3;

    error = xhci_add_interrupter(xhci, xhci->interrupter, 0);

    if (error)
        return error;

    return 0;
}

static int xhci_reset(usb_hcd_t* hcd)
{
    int error;
    uint32_t status;
    uint32_t cmd;
    xhci_hcd_t* xhci = hcd->private;

    status = mmio_read_dword(&xhci->op_regs->status);

    if (status == ~(uint32_t)0)
        return -1;

    if ((status & XHCI_STS_HLT) == 0)
        return -2;

    /* Construct the reset command */
    cmd = mmio_read_dword(&xhci->op_regs->cmd) | XHCI_CMD_RESET;

    /* Set the command */
    mmio_write_dword(&xhci->op_regs->cmd, cmd);

    /*
     * Delay 1 ms to support intel controllers
     * TODO: detect the these intel controllers
     */
    udelay(1000);

    /* Wait for cmd to clear */
    error = xhci_wait_read(hcd, (10 * 1000 * 1000), &xhci->op_regs->cmd, XHCI_CMD_RESET, 0);

    if (error)
        return error;

    /* Wait for cnr to clear */
    error = xhci_wait_read(hcd, (10 * 1000 * 1000), &xhci->op_regs->status, XHCI_STS_CNR, 0);

    if (error)
        return error;

    /* Clear interrupts */
    uint32_t stat = mmio_read_dword(&xhci->op_regs->status);
    mmio_write_dword(&xhci->op_regs->status, stat);
    mmio_write_dword(&xhci->op_regs->device_notif, 0);

    return 0;
}

/*!
 * @brief Make sure bios fucks off
 *
 * Firmware might screw us over, so here we make sure that we have (semi) full control over
 * the host controller
 *
 * We now know that the system might still be in compat mode when we init xhci. If ACPI is enabled
 * and we have successfully initialized ACPICA and it's subcomponents before loading the USB and XHCI
 * hc drivers then this call might not really be needed. Might still be a good idea to call it anyways,
 * since it does not seem to hurt anything
 */
static void xhci_bios_takeover(usb_hcd_t* hcd)
{
    int error;
    uint32_t cparams;
    uint32_t offset;
    uint32_t value, next;
    void* legacy_offset;
    xhci_hcd_t* xhci = hcd_to_xhci(hcd);

    cparams = mmio_read_dword(&xhci->cap_regs->hcc_params_1);
    offset = XHCI_HCC_EXT_CAPS(cparams) << 2;

    do {
        legacy_offset = (void*)xhci->cap_regs + offset;
        value = mmio_read_dword(legacy_offset);

        if (XHCI_EXT_CAP_GETID(value) != XHCI_EXT_CAPS_LEGACY)
            goto cycle;

        if (value & XHCI_HC_BIOS_OWNED) {
            /* Tell xhci we own them */
            mmio_write_dword(legacy_offset, value | XHCI_HC_OS_OWNED);

            error = xhci_wait_read(hcd, 50 * 1000, legacy_offset, XHCI_HC_BIOS_OWNED, 0);

            if (error) {
                /*
                 * Forcefully clear if we fail =)
                 * FIXME: does hardware like this?
                 */
                mmio_write_dword(legacy_offset, value & ~(XHCI_HC_BIOS_OWNED));
            }
        }

        /* Clear SMIs */
        value = mmio_read_dword(legacy_offset + XHCI_LEGACY_CTL_OFF);
        value &= XHCI_LEGACY_DISABLE_SMI;
        value |= XHCI_LEGACY_SMI_EVENTS;
        mmio_write_dword(legacy_offset + XHCI_LEGACY_CTL_OFF, value);
        break;

    cycle:
        next = XHCI_EXT_CAP_NEXT(value);
        offset += next << 2;
    } while (next);
}

static int xhci_force_halt(usb_hcd_t* hcd)
{
    xhci_hcd_t* xhci = hcd->private;

    uint32_t is_halted;
    uint32_t cmd_mask, cmd;

    /* Disable interrupts, engage halt */
    cmd_mask = ~(XHCI_IRQS);
    is_halted = mmio_read_dword(&xhci->op_regs->status) & XHCI_STS_HLT;

    /* The host controller is not yet halted, clear the run bit */
    if (!is_halted) {
        cmd_mask &= ~(XHCI_CMD_RUN);
        xhci->xhc_flags &= ~(XHC_FLAG_HALTED);
    }

    /* Write our halting command field */
    cmd = mmio_read_dword(&xhci->op_regs->cmd) & cmd_mask;
    mmio_write_dword(&xhci->op_regs->cmd, cmd);

    /* Wait for the host to mark itself halted */
    int error = xhci_wait_read(hcd, XHCI_HALT_TIMEOUT_US, &xhci->op_regs->status, XHCI_STS_HLT, XHCI_STS_HLT);

    if (error)
        return error;

    /* Mark halted */
    xhci->xhc_flags |= XHC_FLAG_HALTED;

    return 0;
}

/*!
 * @brief Do HW specific host setup
 *
 * allocate buffers, prepare stuff
 */
int xhci_setup(usb_hcd_t* hcd)
{
    int error;
    paddr_t xhci_register_p;
    size_t xhci_register_size;
    uintptr_t bar0 = 0, bar1 = 0;

    /* Device pointers */
    pci_device_t* device = hcd->pci_device;
    xhci_hcd_t* xhci = hcd->private;

    /* Enable the HC */
    pci_device_enable(device);

    pci_set_interrupt_line(&hcd->pci_device->address, false);
    pci_set_io(&hcd->pci_device->address, false);

    device->ops.read_dword(device, BAR0, (uint32_t*)&bar0);
    device->ops.read_dword(device, BAR1, (uint32_t*)&bar1);

    xhci_register_size = pci_get_bar_size(device, 0);

    xhci_register_p = get_bar_address(bar0);

    if (is_bar_64bit(bar0))
        xhci_register_p |= ((bar1 & 0xFFFFFFFF) << 32);

    /*
     * Prepare by setting the register offsets
     * we can use both the generic mmio functions
     * or the xhci hcd io functions to access these
     * registers
     *
     * TODO: collect allocations by the xhci driver and usb subsystem so we can manage
     * them better lmao
     */
    xhci->register_size = ALIGN_UP(xhci_register_size, SMALL_PAGE_SIZE);
    ASSERT(!__kmem_kernel_alloc(
        (void**)&xhci->register_ptr,
        xhci_register_p, xhci->register_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_DMA));

    xhci->cap_regs_offset = 0;
    xhci->cap_regs = (xhci_cap_regs_t*)xhci->register_ptr;

    xhci->oper_regs_offset = HC_LENGTH(mmio_read_dword(&xhci->cap_regs->hc_capbase));
    xhci->op_regs = (xhci_op_regs_t*)(xhci->register_ptr + XHCI_OP_OF(xhci));

    xhci->runtime_regs_offset = mmio_read_dword(&xhci->cap_regs->runtime_regs_offset) & (~0x1f);
    xhci->runtime_regs = (xhci_runtime_regs_t*)(xhci->register_ptr + XHCI_RR_OF(xhci));

    xhci->doorbell_regs_offset = (mmio_read_dword(&xhci->cap_regs->db_arr_offset) & (~0x3));
    xhci->db_arr = (xhci_db_array_t*)(xhci->register_ptr + xhci->doorbell_regs_offset);

    xhci->hci_version = HC_VERSION(mmio_read_dword(&xhci->cap_regs->hc_capbase));
    xhci->max_interrupters = HC_MAX_INTER(mmio_read_dword(&xhci->cap_regs->hcs_params_1));

    xhci->cmd_queue_cycle = 1;

    uint32_t cap_len = HC_LENGTH(mmio_read_dword(&xhci->cap_regs->hc_capbase));

    /* Make sure BIOS fucks off before we do shit with the controller */
    xhci_bios_takeover(hcd);

    error = xhci_force_halt(hcd);

    if (error)
        goto fail_and_dealloc;

    error = xhci_reset(hcd);

    if (error)
        goto fail_and_dealloc;

    error = xhci_prepare_memory(hcd);

    if (error)
        goto fail_and_dealloc;

    /* Create the async polling threads (TODO: only when it's configured to use this) */
    xhci->event_thread = spawn_thread("xhci_event", SCHED_PRIO_6, _xhci_event_poll, (uint64_t)xhci);
    // xhci->trf_finish_thread = spawn_thread("xhci_trf_thread", nullptr, (uint64_t)xhci);

    /*
     * TODO: support device quirks
     *
     * It seems like no company can make a xhci host controller that is fully compliant
     * with the xhci spec. This really powers up the need to support quirck registering
     * per device. This is also an extra argument for creating a global device structure
     * (which was the plan anyways so yay) that can handle common device occurances.
     * The device structure itself won't enforce how the quick bits are layed out and will
     * leave this to the underlying device
     */

    // kernel_panic(to_string(xhci_register_size));

    kdbgf("XHCI cap len: %d\n", cap_len);
    kdbgf("XHCI version: %d\n", xhci->hci_version);

    kdbgf("XHCI max ports: %d\n", xhci->max_ports);
    kdbgf("XHCI max slots: %d\n", xhci->max_slots);
    kdbgf("XHCI max interrupters: %d\n", xhci->max_interrupters);
    return 0;

fail_and_dealloc:
    /* Since the XHCI registers SHOULD be in a reserved region, this is OK since we are not needing this anymore */
    __kmem_dealloc_unmap(nullptr, nullptr, xhci->register_ptr, xhci_register_size);

    pci_device_disable(device);
    return -1;
}

/*!
 * @brief Start the hcd so it can handle IO requests
 *
 * We make sure the hcd is in a running state and when it is, we will enumerate the devices
 * and make sure they are all powered up. The roothub can have multiple of its own hubs, so
 * we'll detect those and run this 'powerup' sequence recursively. After we have given power
 * to a device, we'll also register it to the usb bus (hcd) so we can have easy access to it
 * and talk to it.
 */
int xhci_start(usb_hcd_t* hcd)
{
    int error;
    uint32_t cmd;
    xhci_interrupter_t* itr;
    xhci_hcd_t* xhci = hcd->private;
    usb_device_t* xhci_rh_dev;

    xhci_rh_dev = create_usb_device(hcd, NULL, USB_SUPERSPEED, 0, 0, "xhci_rh");

    if (!xhci_rh_dev)
        return -1;

    cmd = mmio_read_dword(&xhci->op_regs->cmd);
    cmd |= (XHCI_CMD_EIE);
    mmio_write_dword(&xhci->op_regs->cmd, cmd);

    itr = xhci->interrupter;

    uint32_t ip = mmio_read_dword(&itr->ir_regs->irq_pending);
    mmio_write_dword(&itr->ir_regs->irq_pending, XHCI_ER_IRQ_ENABLE(ip));

    cmd = mmio_read_dword(&xhci->op_regs->cmd);
    cmd |= (XHCI_CMD_RUN);
    mmio_write_dword(&xhci->op_regs->cmd, cmd);

    error = xhci_wait_read(hcd, XHCI_HALT_TIMEOUT_US, &xhci->op_regs->status, XHCI_STS_HLT, 0);

    if (error) {
        println("XHCI hcd took too long to get out of halt state!");

        /* Destroy the device we just created =( */
        destroy_usb_device(xhci_rh_dev);
        return error;
    }

    /*
     * At this point the HCD should be ON and it can accept and process commands.
     * Now we can do a complete enumeration of the ports to find attached devices
     */

    /* Create the xhci roothub and gather device info / power up */
    create_xhci_hub(&xhci->rhub, xhci, xhci_rh_dev);

    return error;
}

/*!
 * @brief Stop the hcd so IO requests will be rejected
 *
 * Nothing to add here...
 */
void xhci_stop(usb_hcd_t* hcd)
{
    kernel_panic("TODO: stop xhci hcd");
    (void)hcd;
}

usb_hcd_hw_ops_t xhci_hw_ops = {
    .hcd_setup = xhci_setup,
    .hcd_start = xhci_start,
    .hcd_stop = xhci_stop,
};

static xhci_hcd_t* create_xhci_hcd(usb_hcd_t* parent)
{
    xhci_hcd_t* hub;

    hub = kmalloc(sizeof(xhci_hcd_t));

    if (!hub)
        return nullptr;

    memset(hub, 0, sizeof(*hub));

    hub->parent = parent;

    return hub;
}

int xhci_probe(pci_device_t* device, pci_driver_t* driver)
{
    int error;
    usb_hcd_t* hcd;
    xhci_hcd_t* xhci_hcd;

    logln("Probing for XHCI");

    /* Create a generic USB hcd */
    hcd = create_usb_hcd(_xhci_driver, device, "xhci", NULL);

    /* Create our own hcd */
    xhci_hcd = create_xhci_hcd(hcd);

    /* Set the hcd methods manually */
    hcd->private = xhci_hcd;
    hcd->hw_ops = &xhci_hw_ops;
    hcd->io_ops = &xhci_io_ops;

    /*
     * Register it to:
     * - Configure the host controller
     * - Configure the roothub
     * - Enumerate the devices
     * - Make it available to the entire system
     */
    error = register_usb_hcd(hcd);

    /* FUCKK */
    if (error)
        return error;

    return 0;
}

int xhci_remove(pci_device_t* device)
{
    pci_driver_t* driver = device->driver;

    /* Huh, thats weird? */
    if (!driver)
        return -1;

    usb_hcd_t* hcd = get_hcd_for_pci_device(device);

    if (!hcd)
        return -1;

    /* TODO: destroy this hcd and unregister it */
    kernel_panic("TODO: implement xhci_remove");

    xhci_hcd_t* xhci = hcd->private;

    xhci_destroy_scratchpad(xhci);

    return 0;
}

/*
 * xhci is hosted through the PCI bus, so we'll register it through that
 */
pci_driver_t xhci_pci_driver = {
    .f_probe = xhci_probe,
    .f_pci_on_remove = xhci_remove,
    .id_table = xhci_pci_ids,
    .device_flags = NULL,
};

/* TODO: finish this driver so we can actually use it ;-; (I hate USB) */
EXPORT_DRIVER(xhci_driver) = {
    .m_name = "xhci",
    .m_descriptor = "XHCI host controller driver",
    .m_type = DT_IO,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = xhci_init,
    .f_exit = xhci_exit,
    .f_msg = xhci_msg,
};

EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};

uintptr_t xhci_msg(aniva_driver_t* this, dcc_t code, void* buffer, size_t size, void* out_buffer, size_t out_size)
{
    return 0;
}

int xhci_init(driver_t* driver)
{
    _xhci_driver = driver;

    register_pci_driver(driver, &xhci_pci_driver);
    return 0;
}

int xhci_exit()
{
    unregister_pci_driver(&xhci_pci_driver);

    return 0;
}
