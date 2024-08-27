#include "drivers/usb/ehci/ehci.h"
#include "dev/pci/pci.h"
#include "dev/usb/hcd.h"
#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "devices/pci.h"
#include "drivers/usb/ehci/ehci_spec.h"
#include "libk/data/linkedlist.h"
#include "libk/data/queue.h"
#include "libk/flow/error.h"
#include "libk/io.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
#include "proc/core.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <stdint.h>

/*
 * EHCI HC driver
 *
 * A few notes:
 * 1) This driver does not take into account the endianness of the HC
 */

static uint32_t _ehci_hcd_count;
static driver_t* _ehci_driver;
static pci_dev_id_t ehci_pci_ids[] = {
    PCI_DEVID_CLASSES(SERIAL_BUS_CONTROLLER, PCI_SUBCLASS_SBC_USB, PCI_PROGIF_EHCI),
    PCI_DEVID_END,
};

static int _ehci_remove_async_qh(ehci_hcd_t* ehci, ehci_qh_t* qh);

static inline uint32_t _ehci_get_portsts(void* portreg)
{
    return mmio_read_dword(portreg) & EHCI_PORTSC_DATAMASK;
}

int ehci_get_port_sts(ehci_hcd_t* ehci, uint32_t port, usb_port_status_t* status)
{
    uint32_t portsts;

    if (port >= ehci->portcount)
        return -1;

    portsts = mmio_read_dword(ehci->opregs + EHCI_OPREG_PORTSC + (port * sizeof(uint32_t)));

    memset(status, 0, sizeof(*status));

    /* Now we need to translate EHCI status to generic USB status =/ */
    if (portsts & EHCI_PORTSC_CONNECT)
        status->status |= USB_PORT_STATUS_CONNECTION;
    if (portsts & EHCI_PORTSC_ENABLE)
        status->status |= (USB_PORT_STATUS_ENABLE | USB_PORT_STATUS_HIGH_SPEED);
    if (portsts & EHCI_PORTSC_OVERCURRENT)
        status->status |= USB_PORT_STATUS_OVER_CURRENT;
    if (portsts & EHCI_PORTSC_RESET)
        status->status |= USB_PORT_STATUS_RESET;
    if (portsts & EHCI_PORTSC_PORT_POWER)
        status->status |= USB_PORT_STATUS_POWER;
    if (portsts & EHCI_PORTSC_SUSPEND)
        status->status |= USB_PORT_STATUS_SUSPEND;
    if (portsts & EHCI_PORTSC_DMINUS_LINESTAT)
        status->status |= USB_PORT_STATUS_LOW_SPEED;

    if ((portsts & EHCI_PORTSC_CONNECT_CHANGE) == EHCI_PORTSC_CONNECT_CHANGE)
        status->change |= USB_PORT_STATUS_CONNECTION;
    if ((portsts & EHCI_PORTSC_ENABLE_CHANGE) == EHCI_PORTSC_ENABLE_CHANGE)
        status->change |= USB_PORT_STATUS_ENABLE;
    if ((portsts & EHCI_PORTSC_OVERCURRENT_CHANGE) == EHCI_PORTSC_OVERCURRENT_CHANGE)
        status->change |= USB_PORT_STATUS_OVER_CURRENT;

    if (ehci->port_reset_bits & (1 << port))
        status->change |= USB_PORT_STATUS_RESET;

    /* TODO: Reset suspend change */

    return 0;
}

/*!
 * @brief: Reset the EHCI port. This will enable the port
 */
static int ehci_reset_port(ehci_hcd_t* ehci, uint32_t port)
{
    void* port_reg;
    uint32_t port_sts;
    bool lowspeed_device;

    KLOG_DBG("[EHCI] Resetting port %d\n", port);

    port_reg = ehci->opregs + EHCI_OPREG_PORTSC + (port * sizeof(uint32_t));
    port_sts = _ehci_get_portsts(port_reg);

    lowspeed_device = (port_sts & EHCI_PORTSC_DMINUS_LINESTAT) == EHCI_PORTSC_DMINUS_LINESTAT;

    if (lowspeed_device)
        goto giveup_maybe_and_exit;

    /* Do the reset */
    port_sts &= ~EHCI_PORTSC_ENABLE;
    port_sts |= EHCI_PORTSC_RESET;
    mmio_write_dword(port_reg, port_sts);

    /* Wait for hardware */
    mdelay(50);

    /* Update */
    port_sts = _ehci_get_portsts(port_reg) & ~EHCI_PORTSC_RESET;
    mmio_write_dword(port_reg, port_sts);

    /* Wait for hardware v2 */
    mdelay(20);

    port_sts = _ehci_get_portsts(port_reg);

    /* Did we successfully reset? */
    if ((port_sts & EHCI_PORTSC_RESET) == EHCI_PORTSC_RESET)
        return -1;

giveup_maybe_and_exit:
    if (lowspeed_device || (port_sts & EHCI_PORTSC_ENABLE) == 0)
        mmio_write_dword(port_reg, port_sts | EHCI_PORTSC_PORT_OWNER);

    ehci->port_reset_bits |= (1 << port);
    return 0;
}

int ehci_set_port_feature(ehci_hcd_t* ehci, uint32_t port, uint16_t feature)
{
    int error;

    if (port >= ehci->portcount)
        return -KERR_INVAL;

    switch (feature) {
    case USB_FEATURE_PORT_RESET:
        error = ehci_reset_port(ehci, port);
        break;
    default:
        error = -KERR_INVAL;
    }

    return error;
}

int ehci_clear_port_feature(ehci_hcd_t* ehci, uint32_t port, uint16_t feature)
{
    uint32_t port_sts;

    if (port >= ehci->portcount)
        return -KERR_INVAL;

    switch (feature) {
    case USB_FEATURE_C_PORT_RESET:
        ehci->port_reset_bits &= ~(1 << port);
        return 0;
    }

    port_sts = _ehci_get_portsts(ehci->opregs + EHCI_OPREG_PORTSC + (port * sizeof(uint32_t)));

    switch (feature) {
    case USB_FEATURE_PORT_ENABLE:
        port_sts &= ~(EHCI_PORTSC_ENABLE);
        break;
    case USB_FEATURE_PORT_POWER:
        port_sts &= ~(EHCI_PORTSC_PORT_POWER);
        break;
    case USB_FEATURE_C_PORT_ENABLE:
        port_sts |= EHCI_PORTSC_ENABLE_CHANGE;
        break;
    case USB_FEATURE_C_PORT_CONNECTION:
        port_sts |= EHCI_PORTSC_CONNECT_CHANGE;
        break;
    case USB_FEATURE_C_PORT_OVER_CURRENT:
        port_sts |= EHCI_PORTSC_OVERCURRENT_CHANGE;
        break;
    }

    mmio_write_dword(ehci->opregs + EHCI_OPREG_PORTSC + (port * sizeof(uint32_t)), port_sts);
    return 0;
}

static int ehci_interrupt_poll(ehci_hcd_t* ehci)
{
    uint32_t usbsts;
    // KLOG_DBG("EHCI: entered the ehci polling routine\n");

    while ((ehci->ehci_flags & EHCI_HCD_FLAG_STOPPING) != EHCI_HCD_FLAG_STOPPING) {

        if (ehci->transfer_list->m_length == 0)
            goto yield_and_cycle;

        usbsts = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS) & 0x3f;

        /*
        if (usbsts & EHCI_OPREG_USBSTS_USBINT) {
          KLOG_DBG("EHCI: Finished a transfer!\n");
        }
        */

        if (usbsts & EHCI_OPREG_USBSTS_USBERRINT)
            kernel_panic("EHCI: Got a transfer error!");

        if (usbsts & EHCI_OPREG_USBSTS_PORTCHANGE)
            KLOG_DBG("EHCI: Port change occured!\n");

        /*
        if (usbsts & EHCI_OPREG_USBSTS_FLROLLOVER) {
            KLOG_DBG("EHCI: Frame List rollover!\n");
        }
        */

        if (usbsts & EHCI_OPREG_USBSTS_HOSTSYSERR)
            kernel_panic("EHCI: Host system error (yikes)!\n");

        if (usbsts & EHCI_OPREG_USBSTS_INTONAA) {
            // KLOG_DBG("EHCI: Advanced the async qh!\n");
            ehci->n_destroyable_qh++;
        }

        mmio_write_dword(ehci->opregs + EHCI_OPREG_USBSTS, usbsts);

    yield_and_cycle:
        scheduler_yield();
    }
    return 0;
}

static inline void _ehci_check_xfer_status(ehci_xfer_t* xfer)
{
    ehci_qtd_t* c_qtd;

    c_qtd = xfer->qh->qtd_link;

    /* Loop over the descriptors to check the status of the transfer */
    do {
        // KLOG_DBG("Qtd: %llx : %s\n", c_qtd->qtd_dma_addr, (c_qtd->hw_token & EHCI_QTD_STATUS_ACTIVE) ? "Active" : "Inactive");

        /* Still busy */
        if (c_qtd->hw_token & EHCI_QTD_STATUS_ACTIVE)
            break;

        /* Error occured in the transfer! Yikes */
        if (c_qtd->hw_token & EHCI_QTD_STATUS_ERRMASK) {
            xfer->xfer->xfer_flags |= (USB_XFER_FLAG_ERROR | USB_XFER_FLAG_DONE);
            break;
        }

        /* Reached the end, transfer done */
        if (c_qtd->hw_next == EHCI_FLLP_TYPE_END) {
            xfer->xfer->xfer_flags |= USB_XFER_FLAG_DONE;
            break;
        }

        c_qtd = c_qtd->next;
    } while (c_qtd);
}

static inline int ehci_get_finished_transfer(ehci_hcd_t* ehci, ehci_xfer_t** p_xfer)
{
    uint64_t idx = 0;
    ehci_xfer_t* c_e_xfer = nullptr;

    FOREACH(i, ehci->transfer_list)
    {
        c_e_xfer = i->data;

        /* Check how the transfer is going */
        _ehci_check_xfer_status(c_e_xfer);

        /* Not done yet, continue */
        if (usb_xfer_is_done(c_e_xfer->xfer))
            break;

        idx++;
    }

    /* Check if we have a finished transfer */
    if (c_e_xfer && usb_xfer_is_done(c_e_xfer->xfer))
        /* Remove from the local transfer list */
        list_remove(ehci->transfer_list, idx);
    else
        /* No finished transfers, cycle */
        c_e_xfer = nullptr;

    /* Export */
    *p_xfer = c_e_xfer;

    /* Nothing to do */
    if (!c_e_xfer)
        return -1;

    return 0;
}

static int ehci_transfer_finish_thread(ehci_hcd_t* ehci)
{
    ehci_xfer_t* c_e_xfer;
    usb_xfer_t* c_usb_xfer;

    while ((ehci->ehci_flags & EHCI_HCD_FLAG_STOPPING) != EHCI_HCD_FLAG_STOPPING) {
        c_e_xfer = nullptr;
        c_usb_xfer = nullptr;

        /* Spin until there are transfers to process */
        while (ehci->transfer_list->m_length == 0)
            scheduler_yield();

        /* Find a finished transfer */
        while (ehci_get_finished_transfer(ehci, &c_e_xfer))
            continue;

        // KLOG_DBG("EHCI: Finished a transfer\n");
        c_usb_xfer = c_e_xfer->xfer;

        ehci_xfer_finalise(ehci, c_e_xfer);

        /* Remove from the async link */
        _ehci_remove_async_qh(ehci, c_e_xfer->qh);

        /* Destroy our local transfer struct */
        destroy_ehci_xfer(ehci, c_e_xfer);

        // KLOG_DBG("Completing: %p\n", c_e_xfer);
        /* Transmit the transfer complete */
        (void)usb_xfer_complete(c_usb_xfer);
    }

    return 0;
}

/*!
 * @brief: Threaded routine to make sure qheads are safely destroyed
 */
static int ehci_qhead_cleanup_thread(ehci_hcd_t* ehci)
{
    ehci_qh_t* to_destroy;

    while ((ehci->ehci_flags & EHCI_HCD_FLAG_STOPPING) != EHCI_HCD_FLAG_STOPPING) {

        /* If there was no recent async advance, we can't destroy anything */
        while (!ehci->n_destroyable_qh)
            scheduler_yield();

        /* Yoink a qh */
        to_destroy = queue_dequeue(ehci->destroyable_qh_q);

        /* Fuck */
        if (!to_destroy)
            continue;

        /* Destroy the qh and clear the async adv flag */
        destroy_ehci_qh(ehci, to_destroy);
        ehci->n_destroyable_qh--;
    }
    return 0;
}

static int ehci_take_bios_ownership(ehci_hcd_t* ehci)
{
    uint32_t legsup;
    uint32_t takeover_tries;
    pci_device_t* dev;
    uint32_t ext_cap = EHCI_GETPARAM_VALUE(ehci->hcc_params, EHCI_HCCPARAMS_EECP, EHCI_HCCPARAMS_EECP_OFFSET);

    if (!ext_cap)
        return 0;

    dev = ehci->hcd->pci_device;

    if (dev->ops.read_dword(dev, ext_cap, &legsup))
        return -1;

    if ((legsup & EHCI_LEGSUP_CAPID_MASK) != EHCI_LEGSUP_CAPID)
        return -1;

    /* Not bios-owned, houray */
    if ((legsup & EHCI_LEGSUP_BIOSOWNED) == 0)
        return 0;

    /* First try */
    takeover_tries = 1;

    KLOG_DBG("We need to take control of EHCI from the BIOS (fuck)\n");

    /* Write a one into the OS-owned byte */
    pci_device_write8(dev, ext_cap + 3, 1);

    /* Keep reading the bios-owned byte */
    do {
        pci_device_read32(dev, ext_cap, &legsup);

        KLOG_DBG("Checking...\n");

        if ((legsup & EHCI_LEGSUP_BIOSOWNED) == 0)
            return 0;

        mdelay(50);
    } while (takeover_tries++ < 32);

    if ((legsup & EHCI_LEGSUP_OSOWNED) == EHCI_LEGSUP_OSOWNED)
        return 0;

    return 0;
}

static int ehci_halt(ehci_hcd_t* ehci)
{
    uint32_t tmp;

    /* Make sure we don't generate shit */
    mmio_write_dword(ehci->opregs + EHCI_OPREG_USBINTR, 0);

    tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
    tmp &= ~(EHCI_OPREG_USBCMD_RS | EHCI_OPREG_USBCMD_INT_ON_ASYNC_ADVANCE_DB);

    mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, tmp);

    /* Wait a bit until the ehci is halted */
    for (uint32_t i = 0; i < 16; i++) {
        tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS);

        if ((tmp & EHCI_OPREG_USBSTS_HCHALTED) == EHCI_OPREG_USBSTS_HCHALTED)
            return 0;

        mdelay(25);
    }

    return -1;
}

static int ehci_reset(ehci_hcd_t* ehci)
{
    uint32_t usbcmd;

    mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, 0);

    /* Sleep a bit until the reset has come through */
    mdelay(10);

    usbcmd = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
    usbcmd |= EHCI_OPREG_USBCMD_HC_RESET;
    mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, usbcmd);

    for (uint32_t i = 0; i < 8; i++) {
        usbcmd = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);

        if ((usbcmd & EHCI_OPREG_USBCMD_HC_RESET) == 0)
            return 0;

        mdelay(50);
    }

    return -1;
}

/*!
 * @brief: Prepare a list of interrupt queueheads
 */
static int ehci_init_periodic_list(ehci_hcd_t* ehci)
{
    ehci_qh_t* c_qh;
    ehci_qh_t* first_int;

    ehci->interrupt_list = (ehci_qh_t**)kmalloc(EHCI_INT_ENTRY_COUNT * sizeof(ehci_qh_t*));

    for (uint32_t i = 0; i < EHCI_INT_ENTRY_COUNT; i++) {
        c_qh = ehci->interrupt_list[i] = zalloc_fixed(ehci->qh_pool);

        /* Assert that the queue head is aligned */
        ASSERT_MSG(((u64)c_qh & 0x1f) == 0, "Allocated a EHCI queue head on a non 32-byte boundry!");

        /* Clear the qh */
        memset(c_qh, 0, sizeof(*c_qh));

        /* Set fields */
        c_qh->qh_dma = kmem_to_phys(nullptr, (vaddr_t)c_qh) | EHCI_FLLP_TYPE_QH;
        c_qh->hw_qtd_token = EHCI_QTD_STATUS_HALTED;
        c_qh->hw_next = EHCI_FLLP_TYPE_END;
        c_qh->hw_qtd_next = EHCI_FLLP_TYPE_END;
        c_qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;

        c_qh->hw_info_0 = (EHCI_QH_FULL_SPEED | EHCI_QH_TOGGLE_CTL | EHCI_QH_MPL(64) | EHCI_QH_RLC(3));
        c_qh->hw_info_1 = (EHCI_QH_INTSCHED(0xff) | EHCI_QH_MULT_VAL(1));
    }

    ehci->n_itd = 128;
    ehci->itd_list = (ehci_itd_t**)kmalloc(ehci->n_itd * sizeof(ehci_itd_t*));
    ehci->sitd_list = (ehci_sitd_t**)kmalloc(ehci->n_itd * sizeof(ehci_sitd_t*));

    /* Initialize the periodic frame list */
    for (uint32_t i = 0; i < ehci->n_itd; i++) {
        /* Allocate a new sitd */
        ehci->sitd_list[i] = zalloc_fixed(ehci->sitd_pool);
        memset(ehci->sitd_list[i], 0, sizeof(ehci_sitd_t));

        /* Initialize it's fields */
        ehci->sitd_list[i]->sitd_dma = kmem_to_phys(nullptr, (vaddr_t)ehci->sitd_list[i]) | EHCI_FLLP_TYPE_SITD;
        ehci->sitd_list[i]->hw_back_buf = EHCI_FLLP_TYPE_END;
        ehci->sitd_list[i]->hw_next = EHCI_FLLP_TYPE_END;

        /* Allocate a new itd */
        ehci->itd_list[i] = zalloc_fixed(ehci->itd_pool);
        memset(ehci->itd_list[i], 0, sizeof(ehci_itd_t));

        ehci->itd_list[i]->itd_dma = kmem_to_phys(nullptr, (vaddr_t)ehci->itd_list[i]) | EHCI_FLLP_TYPE_ITD;
        ehci->itd_list[i]->hw_next = ehci->sitd_list[i]->sitd_dma;
    }

    /* Yoink the first fucker */
    first_int = ehci->interrupt_list[0];
    /* Follow the EHCI spec and link the first periodic entry to the interrupt qh closest to the list */
    ehci->sitd_list[0]->hw_next = first_int->qh_dma;

    for (uint32_t i = 1; i < EHCI_INT_ENTRY_COUNT; i++) {
        c_qh = ehci->interrupt_list[i];

        c_qh->hw_next = first_int->qh_dma;
        c_qh->next = first_int;
        c_qh->prev = nullptr;
    }

    /* Terminate the first entry */
    first_int->hw_next = EHCI_FLLP_TYPE_END;

    uint32_t intr = ehci->n_itd;
    int32_t int_entry_idx = EHCI_INT_ENTRY_COUNT - 1;
    uint32_t sidt_idx;

    /* Fill the sitds with qh at the correct indecies */
    do {
        /* Put the interrupt qheads at the right intervals */
        for (sidt_idx = intr / 2; sidt_idx < ehci->n_itd; sidt_idx += intr)
            ehci->sitd_list[sidt_idx]->hw_next = ehci->interrupt_list[int_entry_idx]->qh_dma;

        if (!int_entry_idx)
            break;

        /* Cut back to the next (or rather previous) interval */
        int_entry_idx--;
        intr /= 2;
    } while (intr > 1);

    /* Finally, put the itds into the periodic list */
    for (uint32_t i = 0; i < ehci->periodic_size; i++)
        ehci->periodic_table[i] = ehci->itd_list[i & (ehci->n_itd - 1)]->itd_dma;

    return 0;
}

/*!
 * @brief: Allocate the structures needed for the function of ehci
 *
 */
static int ehci_init_mem(ehci_hcd_t* ehci)
{
    /* Alignment should be good */
    ehci->qtd_pool = create_zone_allocator(4096, sizeof(ehci_qtd_t), ZALLOC_FLAG_DMA);

    /* Alignment should also be good */
    ehci->qh_pool = create_zone_allocator(4096, sizeof(ehci_qh_t), ZALLOC_FLAG_DMA);

    /* Alignment should also be good */
    ehci->itd_pool = create_zone_allocator(4096, sizeof(ehci_itd_t), ZALLOC_FLAG_DMA);

    /* Alignment should also be good */
    ehci->sitd_pool = create_zone_allocator(4096, sizeof(ehci_sitd_t), ZALLOC_FLAG_DMA);

    /* Allocate the periodic shedule table */
    ASSERT(!__kmem_kernel_alloc_range(
        (void**)&ehci->periodic_table,
        ehci->periodic_size * sizeof(uint32_t),
        NULL, KMEM_FLAG_DMA));
    ehci->periodic_dma = kmem_to_phys(nullptr, (vaddr_t)ehci->periodic_table);

    /* Make sure to terminate all the pariodic frame entries */
    for (uint32_t i = 0; i < ehci->periodic_size; i++)
        ehci->periodic_table[i] = EHCI_FLLP_TYPE_END;

    /* Write the thing lmao */
    mmio_write_dword(ehci->opregs + EHCI_OPREG_PERIODICLISTBASE, ehci->periodic_dma);

    /* Initialize the periodic list for interrupt entries */
    (void)ehci_init_periodic_list(ehci);

    ehci->async = create_ehci_qh(ehci, NULL);

    ehci->async->hw_next = ehci->async->qh_dma;
    ehci->async->next = ehci->async;
    ehci->async->prev = ehci->async;

    ehci->async->hw_info_0 = EHCI_QH_HEAD | EHCI_QH_INACTIVATE;
    ehci->async->hw_qtd_token = EHCI_QTD_STATUS_HALTED;
    ehci->async->hw_qtd_next = EHCI_FLLP_TYPE_END;

    /* Set the async pointer */
    mmio_write_dword(ehci->opregs + EHCI_OPREG_ASYNCLISTADDR, ehci->async->qh_dma);

    return 0;
}

static int ehci_init_interrupts(ehci_hcd_t* ehci)
{
    /* Interrupts are enabled in ehci_start */
    return 0;
}

static int ehci_setup(usb_hcd_t* hcd)
{
    int error;
    ehci_hcd_t* ehci = hcd->private;
    uintptr_t bar0 = 0;
    uint8_t release_num, caplen;

    /* Enable the PCI device */
    pci_device_enable(hcd->pci_device);

    hcd->pci_device->ops.read_dword(hcd->pci_device, BAR0, (uint32_t*)&bar0);

    ehci->register_size = pci_get_bar_size(hcd->pci_device, 0);

    /* Allocate the capability registers */
    ASSERT(!__kmem_kernel_alloc((void**)&ehci->capregs, get_bar_address(bar0), ehci->register_size, NULL, KMEM_FLAG_DMA));

    KLOG_DBG("Setup EHCI registerspace (addr=0x%p, size=0x%llx)\n", ehci->capregs, ehci->register_size);

    hcd->pci_device->ops.read_byte(hcd->pci_device, EHCI_PCI_SBRN, &release_num);

    KLOG_DBG("EHCI release number: 0x%x\n", release_num);

    /* Compute the opregs address */
    caplen = mmio_read_byte(ehci->capregs + EHCI_HCCR_CAPLENGTH);
    ehci->opregs = ehci->capregs + caplen;

    KLOG_DBG("EHCI opregs at 0x%p (caplen=0x%x)\n", ehci->opregs, caplen);

    ehci->hcs_params = mmio_read_dword(ehci->capregs + EHCI_HCCR_HCSPARAMS);
    ehci->hcc_params = mmio_read_dword(ehci->capregs + EHCI_HCCR_HCCPARAMS);
    ehci->portcount = EHCI_GETPARAM_VALUE(ehci->hcs_params, EHCI_HCSPARAMS_N_PORTS, EHCI_HCSPARAMS_N_PORTS_OFFSET) & 0x0f;

    /* Default periodic size (TODO: this may be smaller than the default, detect this) */
    ehci->periodic_size = 1024;

    KLOG_DBG("EHCI HCD portcount: %d\n", ehci->portcount);

    /* Make sure we own this biatch */
    error = ehci_take_bios_ownership(ehci);

    if (error)
        return error;

    /* Reset ourselves */
    error = ehci_reset(ehci);

    if (error)
        return error;

    /* Spec wants us to set the segment register */
    mmio_write_dword(ehci->opregs + EHCI_OPREG_CTRLDSSEGMENT, 0);

    /* Initialize EHCI memory */
    error = ehci_init_mem(ehci);

    if (error)
        return error;

    /* Initialize EHCI interrupt stuff */
    error = ehci_init_interrupts(ehci);

    if (error)
        return error;

    error = ehci_halt(ehci);

    if (error)
        return error;

    ehci->transfer_list = init_list();
    ehci->destroyable_qh_q = create_limitless_queue();
    ehci->transfer_lock = create_mutex(NULL);
    ehci->async_lock = create_mutex(NULL);
    // ehci->cleanup_lock = create_mutex(NULL);
    ehci->transfer_finish_thread = spawn_thread("EHCI Transfer Finisher", SCHED_PRIO_HIGHEST, (FuncPtr)ehci_transfer_finish_thread, (uintptr_t)ehci);
    ehci->qhead_cleanup_thread = spawn_thread("EHCI Qhead cleanup", SCHED_PRIO_HIGHEST, (FuncPtr)ehci_qhead_cleanup_thread, (uintptr_t)ehci);
    /* TODO: Setup actual interrupts */
    ehci->interrupt_polling_thread = spawn_thread("EHCI Polling", SCHED_PRIO_HIGHEST, (FuncPtr)ehci_interrupt_poll, (uint64_t)ehci);

    ASSERT_MSG(ehci->interrupt_polling_thread, "Failed to spawn the EHCI Polling thread!");

    KLOG_DBG("Done with EHCI initialization\n");
    return 0;
}

static int ehci_start(usb_hcd_t* hcd)
{
    uint32_t c_hcstart_spin;
    uint32_t c_tmp;
    ehci_hcd_t* ehci;
    usb_device_t* ehci_rh_device;

    KLOG_DBG("Starting ECHI controller!\n");

    ehci_rh_device = create_usb_device(hcd, nullptr, USB_HIGHSPEED, 0, 0, "ehci_rh");

    /* Fuckkk */
    if (!ehci_rh_device)
        return -1;

    ehci = hcd->private;

    c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
    c_tmp &= ~(
        EHCI_OPREG_USBCMD_HC_RESET | EHCI_OPREG_USBCMD_INT_ON_ASYNC_ADVANCE_DB | EHCI_OPREG_USBCMD_LHC_RESET | EHCI_OPREG_USBCMD_ASYNC_SCHEDULE_ENABLE | EHCI_OPREG_USBCMD_PERIODIC_SCHEDULE_ENABLE);
    c_tmp |= EHCI_OPREG_USBCMD_RS;

    mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, c_tmp);

    /* Wait for the spinup to finish */
    for (c_hcstart_spin = 0; c_hcstart_spin < EHCI_SPINUP_LIMIT; c_hcstart_spin++) {
        c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS);

        if ((c_tmp & EHCI_OPREG_USBSTS_HCHALTED) == 0)
            break;

        mdelay(50);
    }

    if (c_hcstart_spin == EHCI_SPINUP_LIMIT) {
        /* Failed to start, yikes */
        KLOG_DBG("EHCI: Failed to start\n");

        destroy_usb_device(ehci_rh_device);
        return -1;
    }

    c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
    /* Clear interrupt ctl */
    c_tmp &= ~(EHCI_OPREG_USBCMD_INTER_CTL(EHCI_OPREG_USBCMD_INTER_CTL_MASK) | EHCI_OPREG_USBCMD_PPCEE);
    /* Program framelist size */
    c_tmp |= EHCI_OPREG_USBCMD_FRAMELIST_SIZE(((c_tmp >> EHCI_OPREG_USBCMD_FRAMELIST_SIZE_SHIFT) & EHCI_OPREG_USBCMD_FRAMELIST_SIZE_MASK)));
    /* Program interrupt ctl */
    c_tmp |= (1 << EHCI_OPREG_USBCMD_INTER_CTL_SHIFT);
    /* Enable periodic schedule */
    c_tmp |= EHCI_OPREG_USBCMD_PERIODIC_SCHEDULE_ENABLE;

    mmio_write_dword(ehci->opregs + EHCI_OPREG_USBCMD, c_tmp);

    /*
     * Route ports to us from any companion controller(s)
     * NOTE/FIXME: These controllers may be in the middle of a port reset. This
     * may cause some issues...
     * See: https://github.com/torvalds/linux/blob/master/drivers/usb/host/ehci-hcd.c#L573
     */
    mmio_write_dword(ehci->opregs + EHCI_OPREG_CONFIGFLAG, 1);

    /* Flush */
    c_tmp = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);
    mdelay(5);

    ehci->cur_interrupt_state = EHCI_USBINTR_HOSTSYSERR | EHCI_USBINTR_USBERRINT | EHCI_USBINTR_INTONAA | EHCI_USBINTR_PORTCHANGE | EHCI_USBINTR_USBINT;

    /* Write the things to the HC */
    mmio_write_dword(ehci->opregs + EHCI_OPREG_USBINTR, ehci->cur_interrupt_state);

    /* Create this roothub */
    if (create_usb_hub(&hcd->roothub, hcd, USB_HUB_TYPE_EHCI, nullptr, ehci_rh_device, ehci->portcount, false))
        /* No need to destroy the usb device here, since the usb hub owns that memory now and
           create_usb_hub will clean up any memory if it fails */
        return -1;

    /* Enumerate the hub */
    usb_hub_enumerate(hcd->roothub);

    KLOG_DBG("Started the EHCI controller\n");
    return 0;
}

static void ehci_stop(usb_hcd_t* hcd)
{
    (void)hcd;
    kernel_panic("ehci_stop");
}

usb_hcd_hw_ops_t ehci_hw_ops = {
    .hcd_setup = ehci_setup,
    .hcd_start = ehci_start,
    .hcd_stop = ehci_stop,
};

static int _ehci_remove_async_qh(ehci_hcd_t* ehci, ehci_qh_t* qh)
{
    mutex_lock(ehci->async_lock);

    /* Solve the back of the queuehead */
    if (qh->prev) {
        qh->prev->next = qh->next;
        qh->prev->hw_next = qh->hw_next;
    }

    /* And the front */
    if (qh->next)
        qh->next->prev = qh->prev;

    /* Full unlink */
    qh->next = nullptr;
    qh->prev = nullptr;
    qh->hw_next = ehci->async->qh_dma;

    mutex_unlock(ehci->async_lock);
    return 0;
}

static inline void _ehci_try_enable_async(ehci_hcd_t* ehci)
{
    uint32_t cmd;
    uint32_t sts;

    sts = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBSTS);
    cmd = mmio_read_dword(ehci->opregs + EHCI_OPREG_USBCMD);

    /* Already enabled */
    if ((sts & EHCI_OPREG_USBSTS_ASSTATUS) == EHCI_OPREG_USBSTS_ASSTATUS)
        return;

    cmd |= EHCI_OPREG_USBCMD_ASYNC_SCHEDULE_ENABLE | EHCI_OPREG_USBCMD_INT_ON_ASYNC_ADVANCE_DB;
    mmio_write_dword(ehci->opregs, cmd);
}

/*!
 * @brief: Add a qh to the async list
 *
 * Places the new qh right before the current ehci->async
 */
static int _ehci_add_async_qh(ehci_hcd_t* ehci, ehci_qh_t* qh)
{
    mutex_lock(ehci->async_lock);

    qh->hw_next = ehci->async->qh_dma;
    qh->next = ehci->async;
    qh->prev = ehci->async->prev;

    ehci->async->prev = qh;

    qh->prev->next = qh;
    qh->prev->hw_next = qh->qh_dma;

    /* Maybe enable */
    _ehci_try_enable_async(ehci);

    mutex_unlock(ehci->async_lock);
    return 0;
}

static int _ehci_add_async_int_xfer(ehci_hcd_t* ehci, ehci_xfer_t* e_xfer)
{
    uint8_t interval;
    ehci_qh_t *qh, *link_qh;
    usb_xfer_t* xfer;

    xfer = e_xfer->xfer;
    qh = e_xfer->qh;

    interval = xfer->xfer_interval;

    if (interval > EHCI_INT_ENTRY_COUNT)
        interval = EHCI_INT_ENTRY_COUNT;

    // KLOG("Reported interval: %d\n", interval);

    switch (xfer->device->speed) {
    case USB_HIGHSPEED:
        qh->hw_info_1 |= EHCI_QH_INTSCHED(0xff);
        break;
    case USB_LOWSPEED:
        qh->hw_info_1 |= (EHCI_QH_INTSCHED(0x01) | EHCI_QH_SPLITCOMP(0x1c));
        interval = 1;
        break;
    case USB_FULLSPEED:
        qh->hw_info_1 |= (EHCI_QH_INTSCHED(0x01) | EHCI_QH_SPLITCOMP(0x1c));
        interval = 4;
        break;
    default:
        break;
    }

    /* Fuck man */
    ASSERT_MSG(interval, "_ehci_add_async_int_xfer: Got a null-interval");

    // mutex_lock(ehci->async_lock);

    link_qh = ehci->interrupt_list[interval - 1];

    /* Link the new queuehead to the target qh */
    qh->prev = link_qh;
    qh->next = link_qh->next;
    qh->hw_next = link_qh->hw_next;

    /* Make sure the qh we place this new one behind knows about us */
    if (qh->next)
        qh->next->prev = qh;

    /* Finish the link */
    link_qh->next = qh;
    link_qh->hw_next = qh->qh_dma;

    // mutex_unlock(ehci->async_lock);
    return 0;
}

static int _ehci_link_transfer(ehci_hcd_t* ehci, ehci_xfer_t* e_xfer)
{
    if (e_xfer->xfer->req_type == USB_INT_XFER)
        return _ehci_add_async_int_xfer(ehci, e_xfer);

    return _ehci_add_async_qh(ehci, e_xfer->qh);
}

int ehci_enqueue_transfer(usb_hcd_t* hcd, usb_xfer_t* xfer)
{
    int error;
    ehci_qh_t* qh;
    ehci_hcd_t* ehci;
    ehci_xfer_t* e_xfer;
    usb_hub_t* roothub;

    // KLOG_DBG("EHCI: enqueue transfer (%s)!\n", xfer->req_direction == USB_DIRECTION_HOST_TO_DEVICE ? "Outgoing" : "Incomming");

    /*
     * This may happen when we're trying to create the EHCI roothub for this hcd. Should be okay
     */
    if (!hcd->roothub)
        return -1;

    ehci = hcd->private;
    roothub = hcd->roothub;

    if (xfer->req_devaddr == roothub->udev->dev_addr)
        return ehci_process_hub_xfer(roothub, xfer);

    /* Process the EHCI transfer */
    qh = create_ehci_qh(ehci, xfer);

    switch (xfer->req_type) {
    case USB_CTL_XFER:
        error = ehci_init_ctl_queue(ehci, xfer, &e_xfer, qh);
        break;
    default:
        error = ehci_init_data_queue(ehci, xfer, &e_xfer, qh);
        break;
    }

    if (error || !e_xfer)
        goto destroy_and_exit;

    /* Enqueue */
    error = ehci_enq_xfer(ehci, e_xfer);

    if (error)
        goto destroy_and_exit;

    error = _ehci_link_transfer(ehci, e_xfer);

    if (error)
        goto destroy_and_exit;

    return 0;
destroy_and_exit:
    destroy_ehci_qh(ehci, qh);
    return error;
}

usb_hcd_io_ops_t ehci_io_ops = {
    .enq_request = ehci_enqueue_transfer,
};

static ehci_hcd_t* create_ehci_hcd(usb_hcd_t* hcd)
{
    ehci_hcd_t* ehci;

    ehci = kmalloc(sizeof(*ehci));

    if (!ehci)
        return nullptr;

    memset(ehci, 0, sizeof(*ehci));

    ehci->hcd = hcd;

    hcd->private = ehci;
    hcd->hw_ops = &ehci_hw_ops;
    hcd->io_ops = &ehci_io_ops;

    return ehci;
}

/*!
 * @brief: Probe function of the EHCI PCI driver
 *
 * Called when the PCI bus scan finds a device matching our devid list
 */
int ehci_probe(pci_device_t* device, pci_driver_t* driver)
{
    ehci_hcd_t* ehci;
    usb_hcd_t* hcd;
    char hcd_name[16] = { 0 };

    /* Huh */
    if (sfmt(hcd_name, "ehci_hcd%d", _ehci_hcd_count++))
        return -1;

    KLOG_DBG("Found a EHCI device! {\n\tvendorid=0x%x,\n\tdevid=0x%x,\n\tclass=0x%x,\n\tsubclass=0x%x\n}\n", device->vendor_id, device->dev_id, device->class, device->subclass);

    hcd = create_usb_hcd(_ehci_driver, device, hcd_name, NULL);

    if (!hcd)
        return -KERR_NOMEM;

    /* Create the bitch */
    ehci = create_ehci_hcd(hcd);

    if (!ehci)
        goto dealloc_and_exit;

    register_usb_hcd(hcd);
    return 0;

dealloc_and_exit:
    destroy_usb_hcd(hcd);
    return -KERR_INVAL;
}

int ehci_remove(pci_device_t* device)
{
    return 0;
}

/*
 * xhci is hosted through the PCI bus, so we'll register it through that
 */
pci_driver_t ehci_pci_driver = {
    .f_probe = ehci_probe,
    .f_pci_on_remove = ehci_remove,
    .id_table = ehci_pci_ids,
    .device_flags = NULL,
};

int ehci_init(driver_t* driver)
{
    _ehci_hcd_count = 0;
    _ehci_driver = driver;

    ehci_init_xfer();

    register_pci_driver(driver, &ehci_pci_driver);

    return 0;
}

int ehci_exit()
{
    kernel_panic("TODO: Remove all the child devices created by this driver");
    unregister_pci_driver(&ehci_pci_driver);
    return 0;
}

EXPORT_DRIVER(ehci_hcd) = {
    .m_name = "ehci",
    .m_descriptor = "EHCI host controller driver",
    .m_type = DT_IO,
    .m_version = DRIVER_VERSION(0, 0, 1),
    .f_init = ehci_init,
    .f_exit = ehci_exit,
};

/* No dependencies */
EXPORT_DEPENDENCIES(deps) = {
    DRV_DEP_END,
};
