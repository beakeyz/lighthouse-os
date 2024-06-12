#include "dev/usb/spec.h"
#include "dev/usb/usb.h"
#include "dev/usb/xfer.h"
#include "drivers/usb/ehci/ehci_spec.h"
#include "ehci.h"
#include "libk/data/linkedlist.h"
#include "libk/data/queue.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
#include "sync/mutex.h"

#define EHCI_MAX_PACKETSIZE (4 * SMALL_PAGE_SIZE)

ehci_xfer_t* create_ehci_xfer(struct usb_xfer* xfer, ehci_qh_t* qh, ehci_qtd_t* data_qtd)
{
    ehci_xfer_t* e_xfer;

    e_xfer = kmalloc(sizeof(*e_xfer));

    if (!e_xfer)
        return nullptr;

    memset(e_xfer, 0, sizeof(*e_xfer));

    e_xfer->xfer = xfer;
    e_xfer->qh = qh;
    e_xfer->data_qtd = data_qtd;

    return e_xfer;
}

void destroy_ehci_xfer(ehci_hcd_t* ehci, ehci_xfer_t* xfer)
{
    mutex_lock(ehci->cleanup_lock);

    /* Enqueue the queue head so we can kill it peacefully */
    queue_enqueue(ehci->destroyable_qh_q, xfer->qh);

    if (xfer->xfer->req_type == USB_INT_XFER)
        ehci->ehci_flags |= EHCI_HCD_FLAG_CAN_DESTROY_QH;

    mutex_unlock(ehci->cleanup_lock);

    // destroy_ehci_qh(ehci, xfer->qh);
    kfree(xfer);
}

int ehci_enq_xfer(ehci_hcd_t* ehci, ehci_xfer_t* xfer)
{
    mutex_lock(ehci->transfer_lock);

    list_append(ehci->transfer_list, xfer);

    mutex_unlock(ehci->transfer_lock);
    return 0;
}

/*!
 * @brief: Remove a transfer from the ehci hcds queue
 */
int ehci_deq_xfer(ehci_hcd_t* ehci, ehci_xfer_t* xfer)
{
    bool remove_success;

    mutex_lock(ehci->transfer_lock);

    remove_success = list_remove_ex(ehci->transfer_list, xfer);

    mutex_unlock(ehci->transfer_lock);

    if (remove_success)
        return 0;

    return -1;
}

static inline void _try_allocate_qtd_buffer(ehci_hcd_t* ehci, ehci_qtd_t* qtd)
{
    paddr_t c_phys;

    if (!qtd->len)
        return;

    qtd->buffer = (void*)Must(__kmem_kernel_alloc_range(qtd->len, NULL, KMEM_FLAG_DMA));

    memset(qtd->buffer, 0, qtd->len);

    /* This allocation should be page aligned, so we're all good on that part */
    c_phys = kmem_to_phys(nullptr, (vaddr_t)qtd->buffer);

    for (uint32_t i = 0; i < 5; i++) {
        qtd->hw_buffer[i] = (c_phys & EHCI_QTD_PAGE_MASK);
        qtd->hw_buffer_hi[i] = NULL;

        c_phys += SMALL_PAGE_SIZE;
    }
}

static ehci_qtd_t* _create_ehci_qtd_raw(ehci_hcd_t* ehci, size_t bsize, uint8_t pid)
{
    paddr_t this_dma;
    ehci_qtd_t* ret;

    ret = zalloc_fixed(ehci->qtd_pool);

    if (!ret)
        return nullptr;

    memset(ret, 0, sizeof(*ret));

    this_dma = kmem_to_phys(nullptr, (vaddr_t)ret);

    ret->hw_next = EHCI_FLLP_TYPE_END;
    ret->hw_alt_next = EHCI_FLLP_TYPE_END;

    /* Token is kinda like a flags field */
    ret->hw_token = (bsize << EHCI_QTD_BYTES_SHIFT) | (3 << EHCI_QTD_ERRCOUNT_SHIFT) | (pid << EHCI_QTD_PID_SHIFT) | EHCI_QTD_STATUS_ACTIVE;

    ret->qtd_dma_addr = this_dma;
    ret->len = bsize;

    _try_allocate_qtd_buffer(ehci, ret);

    return ret;
}

static void _destroy_ehci_qtd(ehci_hcd_t* ehci, ehci_qtd_t* qtd)
{
    __kmem_kernel_dealloc((vaddr_t)qtd->buffer, qtd->len);
    zfree_fixed(ehci->qtd_pool, qtd);
}

void ehci_init_qh(ehci_qh_t* qh, usb_xfer_t* xfer)
{
    size_t max_pckt_size;

    switch (xfer->device->speed) {
    case USB_LOWSPEED:
        qh->hw_info_0 = EHCI_QH_LOW_SPEED;
        break;
    case USB_FULLSPEED:
        qh->hw_info_0 = EHCI_QH_FULL_SPEED;
        break;
    case USB_HIGHSPEED:
        qh->hw_info_0 = EHCI_QH_HIGH_SPEED;
        break;
    default:
        /* This is bad lol */
        break;
    }

    (void)usb_xfer_get_max_packet_size(xfer, &max_pckt_size);

    qh->hw_info_0 |= (EHCI_QH_DEVADDR(xfer->req_devaddr) | EHCI_QH_EP_NUM(xfer->req_endpoint) | EHCI_QH_MPL(max_pckt_size) | EHCI_QH_TOGGLE_CTL);
    qh->hw_info_1 = EHCI_QH_MULT_VAL(1);

    if (xfer->device->speed == USB_HIGHSPEED)
        return;

    qh->hw_info_0 |= (xfer->req_type == USB_CTL_XFER ? EHCI_QH_CONTROL_EP : 0);
    qh->hw_info_1 |= (EHCI_QH_HUBADDR(xfer->req_hubaddr) | EHCI_QH_HUBPORT(xfer->req_hubport));
}

ehci_qh_t* create_ehci_qh(ehci_hcd_t* ehci, struct usb_xfer* xfer)
{
    paddr_t this_dma;
    ehci_qtd_t* qtd;
    ehci_qh_t* qh;

    qh = zalloc_fixed(ehci->qh_pool);

    if (!qh)
        return nullptr;

    this_dma = kmem_to_phys(nullptr, (vaddr_t)qh);

    /* Sanity */
    ASSERT_MSG(ALIGN_UP((uint64_t)this_dma, 32) == (uint64_t)this_dma, "EHCI: Got an unaligned qh!");

    /* Zero the thing, just to be sure */
    memset(qh, 0, sizeof(*qh));

    /* Try to create the initial qtd for this qh */
    qtd = _create_ehci_qtd_raw(ehci, NULL, NULL);

    if (!qtd) {
        zfree_fixed(ehci->qh_pool, qh);
        return nullptr;
    }

    /* This is not an active qtd */
    qtd->hw_token &= ~EHCI_QTD_STATUS_ACTIVE;

    /* Hardware init for the qh */
    qh->hw_cur_qtd = NULL;
    qh->hw_next = EHCI_FLLP_TYPE_END;

    /* Hardware init for the qtd overlay thingy */
    qh->hw_qtd_next = qtd->qtd_dma_addr;
    qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;

    /* Software init */
    qh->qh_dma = this_dma | EHCI_FLLP_TYPE_QH;
    qh->next = nullptr;
    qh->prev = nullptr;
    qh->qtd_link = qtd;
    qh->qtd_alt = qtd;

    if (xfer)
        ehci_init_qh(qh, xfer);

    return qh;
}

void destroy_ehci_qh(ehci_hcd_t* ehci, ehci_qh_t* qh)
{
    ehci_qtd_t *this_qtd, *next_qtd;

    this_qtd = qh->qtd_link;

    /* Murder this fucker if it is not part of the link */
    if (qh->qtd_alt != this_qtd)
        _destroy_ehci_qtd(ehci, qh->qtd_alt);

    while (this_qtd) {
        next_qtd = this_qtd->next;

        /* Destroy this fucker */
        _destroy_ehci_qtd(ehci, this_qtd);

        /* Go next */
        this_qtd = next_qtd;
    }

    /* Return to pool */
    zfree_fixed(ehci->qh_pool, qh);
}

ehci_qtd_t* create_ehci_qtd(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_qh_t* qh);

/*!
 * @brief: Write to a queue transfer chain
 *
 * @returns: The amount of bytes that have been written to the chain
 */
static size_t _ehci_write_qtd_chain(ehci_hcd_t* ehci, ehci_qtd_t* qtd, ehci_qtd_t** blast_qtd, void* buffer, size_t bsize)
{
    size_t written_size;
    size_t c_written_size;
    ehci_qtd_t* c_qtd;

    if (!ehci || !qtd)
        return NULL;

    c_qtd = qtd;
    written_size = NULL;

    do {
        c_written_size = bsize > qtd->len ? qtd->len : bsize;

        /* Copy the bytes */
        memcpy(qtd->buffer, buffer + written_size, c_written_size);

        /* Mark the written bytes */
        written_size += c_written_size;

        if (c_qtd->hw_next & EHCI_FLLP_TYPE_END)
            break;

        if (written_size >= bsize)
            break;

        c_qtd = c_qtd->next;
    } while (c_qtd);

    if (blast_qtd)
        *blast_qtd = c_qtd;

    return written_size;
}

static void _ehci_link_qtds(ehci_qtd_t* a, ehci_qtd_t* b, ehci_qtd_t* alt)
{
    a->next = b;
    a->hw_next = b->qtd_dma_addr;

    /* Set alt descriptor */
    a->alt_next = (alt ? alt : NULL);
    a->hw_alt_next = (alt ? alt->qtd_dma_addr : EHCI_FLLP_TYPE_END);
}

static void _ehci_xfer_attach_data_qtds(ehci_hcd_t* ehci, ehci_qtd_t* alt, size_t bsize, enum USB_XFER_DIRECTION direction, ehci_qtd_t** start, ehci_qtd_t** end)
{
    // Bind more descriptors to include the data
    ehci_qtd_t* b;
    ehci_qtd_t* _start;
    ehci_qtd_t* _end;
    size_t packet_count;
    size_t c_packet_size;

    _start = NULL;
    _end = NULL;
    packet_count = ALIGN_UP(bsize, EHCI_MAX_PACKETSIZE) / EHCI_MAX_PACKETSIZE;

    for (size_t i = 0; i < packet_count; i++) {
        c_packet_size = (bsize - i * EHCI_MAX_PACKETSIZE) % EHCI_MAX_PACKETSIZE;

        b = _create_ehci_qtd_raw(ehci, c_packet_size, direction == USB_DIRECTION_DEVICE_TO_HOST ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT);

        if (!_start)
            _start = b;

        b->hw_token |= EHCI_QTD_DATA_TOGGLE;

        if (_end)
            _ehci_link_qtds(_end, b, alt);

        _end = b;
    }

    *start = _start;
    *end = _end;
}

int ehci_init_ctl_queue(ehci_hcd_t* ehci, struct usb_xfer* xfer, ehci_xfer_t** e_xfer, ehci_qh_t* qh)
{
    ehci_qtd_t *setup_desc, *stat_desc;
    ehci_qtd_t *data_start, *data_end;
    usb_ctlreq_t* ctl;

    ctl = xfer->req_buffer;

    setup_desc = _create_ehci_qtd_raw(ehci, sizeof(*ctl), EHCI_QTD_PID_SETUP);
    stat_desc = _create_ehci_qtd_raw(ehci, 0, xfer->req_direction == USB_DIRECTION_DEVICE_TO_HOST ? EHCI_QTD_PID_OUT : EHCI_QTD_PID_IN);

    data_start = setup_desc;
    data_end = setup_desc;

    /* Try to write this bitch */
    if (_ehci_write_qtd_chain(ehci, setup_desc, NULL, ctl, sizeof(*ctl)) != sizeof(*ctl))
        goto dealloc_and_exit;

    stat_desc->hw_token |= EHCI_QTD_IOC | EHCI_QTD_DATA_TOGGLE;

    /* If there is a buffer, create a data segment in the packet chain */
    if (xfer->resp_buffer)
        _ehci_xfer_attach_data_qtds(ehci, stat_desc, xfer->resp_size, xfer->req_direction, &data_start, &data_end);

    if (data_start != setup_desc)
        _ehci_link_qtds(setup_desc, data_start, qh->qtd_alt);

    _ehci_link_qtds(data_end, stat_desc, qh->qtd_alt);

    qh->qtd_link = setup_desc;
    qh->hw_qtd_next = setup_desc->qtd_dma_addr;
    qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;

    /* Create the EHCI transfer */
    *e_xfer = create_ehci_xfer(xfer, qh, (data_start != setup_desc) ? data_start : NULL);
    return 0;

dealloc_and_exit:
    kernel_panic("TODO: ehci_init_ctl_queue dealloc_and_exit");
    return -1;
}

/*!
 * @brief: Create a transfer queue for data
 *
 * Called from the enqueue_transfer routine, which manages qh memory, so we don't have to be weird with destroying things we
 * create as long as we attach them correctly
 */
int ehci_init_data_queue(ehci_hcd_t* ehci, struct usb_xfer* xfer, struct ehci_xfer** e_xfer, ehci_qh_t* qh)
{
    size_t write_size;
    ehci_qtd_t *start, *end;

    start = end = nullptr;

    /* Create a descriptor chain */
    _ehci_xfer_attach_data_qtds(ehci, qh->qtd_alt, xfer->req_size, xfer->req_direction, &start, &end);

    if (!start || !end)
        return -KERR_INVAL;

    /* Interrupt when we reach this qtd */
    end->hw_token |= EHCI_QTD_IOC;

    if (xfer->req_direction == USB_DIRECTION_HOST_TO_DEVICE) {
        write_size = _ehci_write_qtd_chain(ehci, start, NULL, xfer->req_buffer, xfer->req_direction);

        if (write_size != xfer->req_size)
            return -KERR_INVAL;
    } else if (!xfer->resp_buffer || !xfer->resp_size)
        return -KERR_INVAL;

    qh->qtd_link = start;
    qh->hw_qtd_next = start->qtd_dma_addr;
    qh->hw_qtd_alt_next = EHCI_FLLP_TYPE_END;

    /* Create the EHCI transfer */
    *e_xfer = create_ehci_xfer(xfer, qh, start);
    return 0;
}

/*!
 * @brief: Called right before the usb_xfer is marked completed
 */
int ehci_xfer_finalise(ehci_hcd_t* ehci, ehci_xfer_t* xfer)
{
    uintptr_t c_buffer_offset;
    uintptr_t c_read_size;
    uintptr_t c_total_read_size;
    ehci_qtd_t* c_qtd;
    usb_xfer_t* usb_xfer;

    c_buffer_offset = NULL;
    c_read_size = NULL;
    c_total_read_size = NULL;
    usb_xfer = xfer->xfer;

    switch (usb_xfer->req_direction) {
    /* Device might have given us info, populate the response buffer */
    case USB_DIRECTION_DEVICE_TO_HOST:
        c_qtd = xfer->data_qtd;

        while (c_qtd && c_qtd->buffer) {
            /* Get the buffer size of this descriptor */
            c_read_size = c_qtd->len - (c_qtd->hw_token >> EHCI_QTD_BYTES_SHIFT) & EHCI_QTD_BYTES_MASK;

            /* Clamp */
            if ((c_total_read_size + c_read_size) > xfer->xfer->resp_size)
                c_read_size = xfer->xfer->resp_size - c_buffer_offset;

            /* Copy to the response buffer */
            memcpy(xfer->xfer->resp_buffer + c_buffer_offset, c_qtd->buffer, c_read_size);

            // printf("Copied %lld bytes into the buffer!\n", c_read_size);

            /* Update the offsets */
            c_total_read_size += c_read_size;
            c_buffer_offset += c_read_size;

            /* Buffer overrun! yikes */
            if (c_buffer_offset >= xfer->xfer->resp_size)
                break;

            if (c_qtd->hw_next == EHCI_FLLP_TYPE_END)
                break;

            c_qtd = c_qtd->next;
        }

        /* Update the total transfer size */
        usb_xfer->resp_transfer_size = c_total_read_size;
        break;
    /* We have sent info to the device, check status */
    case USB_DIRECTION_HOST_TO_DEVICE:
        break;
    }
    return 0;
}
