#include "dev/usb/spec.h"
#include "dev/usb/xfer.h"
#include "libk/flow/error.h"
#include "xhci.h"
#include <dev/usb/hcd.h>

static usb_device_descriptor_t __xhci_rh_descriptor = {
    .type = USB_DT_DEVICE,
    .length = sizeof(usb_device_descriptor_t),
    .bcd_usb = 0x300,
    .dev_class = 0x9,
    .dev_subclass = 0,
    .dev_prot = 3,
    .max_pckt_size = 9,
    .vendor_id = 0,
    .product_id = 0,
    .bcd_device = 0x300,
    .manufacturer = 1,
    .product = 2,
    .serial_num = 0,
    .config_count = 1,
};

struct {
    usb_configuration_descriptor_t desc;
    usb_interface_descriptor_t interface;
    usb_endpoint_descriptor_t ep;
    usb_hub_descriptor_t hub;
} __attribute__((packed)) __xhci_rh_config_descriptor = {
    {
        .hdr = {
            .length = 0x09,
            .type = USB_DT_CONFIG,
        },
        .total_len = 0x09,
        .if_num = 0x01,
        .config_value = 0x01,
        .config_index = 0x00,
        .bm_attr = 0x40,
        .max_power_mA = 0,
    },
    { .hdr = {
          .length = sizeof(usb_interface_descriptor_t),
          .type = USB_DT_INTERFACE,
      },
        .interface_number = 0,
        .alternate_setting = 0,
        .num_endpoints = 1,
        .interface_class = 0x09,
        .interface_subclass = 0,
        .interface_protocol = 0,
        .interface_str = 0 },
    {
        .hdr = {
            .length = sizeof(usb_endpoint_descriptor_t),
            .type = USB_DT_ENDPOINT,
        },
        .endpoint_address = USB_TYPE_DEV_IN | 1,
        .attributes = 0x03,
        .max_packet_size = 2,
        .interval = 0xff,
    },
    {
        .hdr = {
            .length = sizeof(usb_hub_descriptor_t),
            .type = USB_DT_HUB,
        },
        .portcount = 15,
        .characteristics = 0,
        .power_stabilize_delay_2ms = 10,
        .max_power_mA = 0,
        .removeable = 0,
        .power_ctl_mask = 0xff,
    }

};

int xhci_process_rhub_xfer(usb_hub_t* hub, usb_xfer_t* xfer)
{
    int error;
    uint32_t idx;
    xhci_hcd_t* xhci;
    usb_ctlreq_t* usbctl;

    /* Hub can only process ctl transfers */
    if (xfer->req_type != USB_CTL_XFER)
        return -KERR_INVAL;

    xhci = hub->hcd->private;

    if (!xhci)
        return -KERR_INVAL;

    usbctl = xfer->req_buffer;

    if (!usbctl)
        return -KERR_INVAL;

    /* Fail by default */
    error = -KERR_INVAL;

    idx = (usbctl->index - 1) & 0xff;

    if (idx >= XHCI_MAX_HC_PORTS)
        idx = 0;

    printf("Got XHCI Roothub transfer of type %d\n", usbctl->request);

    switch (usbctl->request) {
    case USB_REQ_GET_STATUS:
        /* We're going to need a response destination ='[ */
        if (!xfer->resp_buffer || xfer->resp_size != sizeof(usb_port_status_t))
            break;

        xfer->req_tranfered_size = sizeof(usb_port_status_t);

        /* Report our own status (all zero = OK) */
        memset(xfer->resp_buffer, 0, xfer->resp_size);

        error = KERR_NONE;

        if (usbctl->index)
            error = xhci_get_port_sts(xhci, idx, xfer->resp_buffer);

        break;
    case USB_REQ_CLEAR_FEATURE:
        if (!usbctl->index)
            break;

        error = xhci_clear_port_ftr(xhci, idx, usbctl->value);
        break;
    case USB_REQ_SET_FEATURE:
        if (!usbctl->index)
            break;

        error = xhci_set_port_ftr(xhci, idx, usbctl->value);
        break;
    case USB_REQ_GET_DESCRIPTOR:
        if (!xfer->resp_buffer || !xfer->resp_size)
            break;

        uint32_t cpy_size = xfer->resp_size;

        switch (usbctl->value >> 8) {
        case USB_DT_DEVICE:
            if (cpy_size > sizeof(__xhci_rh_descriptor))
                cpy_size = sizeof(__xhci_rh_descriptor);

            /* Copy the descriptor */
            memcpy(xfer->resp_buffer, &__xhci_rh_descriptor, cpy_size);
            error = 0;
            break;
        case USB_DT_HUB:
            if (cpy_size > sizeof(__xhci_rh_config_descriptor.hub))
                cpy_size = sizeof(__xhci_rh_config_descriptor.hub);

            /* Update the portcount */
            __xhci_rh_config_descriptor.hub.portcount = xhci->max_ports;

            /* Copy the descriptor */
            memcpy(xfer->resp_buffer, &__xhci_rh_config_descriptor.hub, cpy_size);
            error = 0;
            break;
        case USB_DT_CONFIG:
            if (cpy_size > sizeof(__xhci_rh_config_descriptor.desc))
                cpy_size = sizeof(__xhci_rh_config_descriptor.desc);

            /* Update the portcount */
            __xhci_rh_config_descriptor.hub.portcount = xhci->max_ports;

            /* Copy the descriptor */
            memcpy(xfer->resp_buffer, &__xhci_rh_config_descriptor.desc, cpy_size);
            error = 0;
            break;
        }
        break;
    case USB_REQ_SET_CONFIGURATION:
        error = 0;
        break;
    default:
        break;
    }

    usb_xfer_complete(xfer);
    return error;
}
