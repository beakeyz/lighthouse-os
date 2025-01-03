#ifndef __ANIVA_USB_SPECIFICATION__
#define __ANIVA_USB_SPECIFICATION__

#include <libk/stddef.h>
#include <libk/string.h>

struct usb_interface_buffer;
struct usb_endpoint_buffer;

/*
 * Standard requests, for the bRequest field of a SETUP packet.
 *
 * These are qualified by the bRequestType field, so that for example
 * TYPE_CLASS or TYPE_VENDOR specific feature flags could be retrieved
 * by a GET_STATUS request.
 *
 * Yoinked from: https://github.com/torvalds/linux/blob/master/include/uapi/linux/usb/ch9.h
 */
#define USB_REQ_GET_STATUS 0x00
#define USB_REQ_CLEAR_FEATURE 0x01
#define USB_REQ_SET_FEATURE 0x03
#define USB_REQ_SET_ADDRESS 0x05
#define USB_REQ_GET_DESCRIPTOR 0x06
#define USB_REQ_SET_DESCRIPTOR 0x07
#define USB_REQ_GET_CONFIGURATION 0x08
#define USB_REQ_SET_CONFIGURATION 0x09
#define USB_REQ_GET_INTERFACE 0x0A
#define USB_REQ_SET_INTERFACE 0x0B
#define USB_REQ_SYNCH_FRAME 0x0C
#define USB_REQ_SET_SEL 0x30
#define USB_REQ_SET_ISOCH_DELAY 0x31
#define USB_REQ_SET_ENCRYPTION 0x0D /* Wireless USB */
#define USB_REQ_GET_ENCRYPTION 0x0E
#define USB_REQ_RPIPE_ABORT 0x0E
#define USB_REQ_SET_HANDSHAKE 0x0F
#define USB_REQ_RPIPE_RESET 0x0F
#define USB_REQ_GET_HANDSHAKE 0x10
#define USB_REQ_SET_CONNECTION 0x11
#define USB_REQ_SET_SECURITY_DATA 0x12
#define USB_REQ_GET_SECURITY_DATA 0x13
#define USB_REQ_SET_WUSB_DATA 0x14
#define USB_REQ_LOOPBACK_DATA_WRITE 0x15
#define USB_REQ_LOOPBACK_DATA_READ 0x16
#define USB_REQ_SET_INTERFACE_DS 0x17

/* specific requests for USB Power Delivery */
#define USB_REQ_GET_PARTNER_PDO 20
#define USB_REQ_GET_BATTERY_STATUS 21
#define USB_REQ_SET_PDO 22
#define USB_REQ_GET_VDM 23
#define USB_REQ_SEND_VDM 24

/* specific request for HID devices */
#define USB_REQ_GET_REPORT 0x01
#define USB_REQ_GET_IDLE 0x02
#define USB_REQ_GET_PROTOCOL 0x03
#define USB_REQ_SET_REPORT 0x09
#define USB_REQ_SET_IDLE 0x0A
#define USB_REQ_SET_PROTOCOL 0x0B

/* Incomming means: device to host */
#define USB_DIRECTION_IN (0x80)
/* Outgoing means: host to device */
#define USB_DIRECTION_OUT (0x00)

#define USB_TYPE_DEV_IN USB_DIRECTION_IN
#define USB_TYPE_DEV_OUT USB_DIRECTION_OUT
#define USB_TYPE_IF_IN (0x81)
#define USB_TYPE_IF_OUT (0x01)
#define USB_TYPE_EP_IN (0x82)
#define USB_TYPE_EP_OUT (0x02)
#define USB_TYPE_OTHER_IN (0x83)
#define USB_TYPE_OTHER_OUT (0x03)

#define USB_TYPE_MASK (0x03 << 5)
#define USB_TYPE_STANDARD (0x00 << 5)
#define USB_TYPE_CLASS (0x01 << 5)
#define USB_TYPE_VENDOR (0x02 << 5)
#define USB_TYPE_RESERVED (0x03 << 5)

/*
 * Simple structure of a usb control request
 */
typedef struct usb_ctlreq {
    uint8_t request_type;
    uint8_t request;
    uint16_t value;
    uint16_t index;
    uint16_t length;
} __attribute__((packed)) usb_ctlreq_t;

/*
 * USB Descriptor types
 */
#define USB_DT_DEVICE 0x01
#define USB_DT_CONFIG 0x02
#define USB_DT_STRING 0x03
#define USB_DT_INTERFACE 0x04
#define USB_DT_ENDPOINT 0x05
#define USB_DT_DEVICE_QUALIFIER 0x06
#define USB_DT_OTHER_SPEED_CONFIG 0x07
#define USB_DT_INTERFACE_POWER 0x08
#define USB_DT_OTG 0x09
#define USB_DT_DEBUG 0x0a
#define USB_DT_INTERFACE_ASSOCIATION 0x0b
#define USB_DT_SECURITY 0x0c
#define USB_DT_KEY 0x0d
#define USB_DT_ENCRYPTION_TYPE 0x0e
#define USB_DT_BOS 0x0f
#define USB_DT_DEVICE_CAPABILITY 0x10
#define USB_DT_WIRELESS_ENDPOINT_COMP 0x11
#define USB_DT_WIRE_ADAPTER 0x21
#define USB_DT_RPIPE 0x22
#define USB_DT_CS_RADIO_CONTROL 0x23
#define USB_DT_PIPE_USAGE 0x24
#define USB_DT_HUB 0x29
#define USB_DT_SS_ENDPOINT_COMP 0x30
#define USB_DT_SSP_ISOC_ENDPOINT_COMP 0x31

typedef struct usb_descriptor_hdr {
    uint8_t length;
    uint8_t type;
} __attribute__((packed)) usb_descriptor_hdr_t;

typedef struct usb_device_descriptor {
    uint8_t length;
    uint8_t type;

    uint16_t bcd_usb;
    uint8_t dev_class;
    uint8_t dev_subclass;
    uint8_t dev_prot;
    uint8_t max_pckt_size;
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t bcd_device;
    uint8_t manufacturer;
    uint8_t product;
    uint8_t serial_num;
    uint8_t config_count;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct usb_device_qualifier_descriptor {
    usb_descriptor_hdr_t hdr;
    uint16_t bcd_usb;
    uint8_t dev_class;
    uint8_t dev_subclass;
    uint8_t dev_prot;
    uint8_t max_pckt_size;
    uint8_t config_count;
    uint8_t reserved;
} __attribute__((packed)) usb_device_qualifier_descriptor_t;

typedef struct usb_configuration_descriptor {
    usb_descriptor_hdr_t hdr;
    uint16_t total_len;
    uint8_t if_num;
    uint8_t config_value;
    uint8_t config_index;
    uint8_t bm_attr;
    uint8_t max_power_mA;
} __attribute__((packed)) usb_configuration_descriptor_t;

/*
 * Our buffer we use to store configuration Descriptors in usb device objects
 */
typedef struct usb_configuration_buffer {
    uint32_t total_len;
    uint16_t if_count;
    uint16_t active_if;
    struct usb_interface_buffer* if_arr;

    usb_configuration_descriptor_t desc;
    uint8_t extended_desc[];
} __attribute__((packed)) usb_config_buffer_t;

typedef struct usb_interface_descriptor {
    usb_descriptor_hdr_t hdr;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t num_endpoints;
    uint8_t interface_class;
    uint8_t interface_subclass;
    uint8_t interface_protocol;
    uint8_t interface_str;
} __attribute__((packed)) usb_interface_descriptor_t;

/*
 * An actual interface inside the buffer
 */
typedef struct usb_interface_entry {
    usb_interface_descriptor_t desc;

    struct usb_endpoint_buffer* ep_list;
    struct usb_interface_entry* next;
} usb_interface_entry_t;

static inline unsigned int usb_interface_get_ep_count(usb_interface_entry_t* ife)
{
    return ife->desc.num_endpoints;
}

/*
 * Buffer to store interface entries for a specific configuration
 *
 * Stored in an array most likely
 */
typedef struct usb_interface_buffer {
    uint8_t alt_count;
    uint8_t config_val;
    usb_interface_entry_t* alt_list;
} usb_interface_buffer_t;

static inline usb_interface_entry_t** usb_if_buffer_get_last_entry(usb_interface_buffer_t* if_buf)
{
    usb_interface_entry_t** ret;

    for (ret = &if_buf->alt_list; *ret; ret = &(*ret)->next)
        ;

    return ret;
}

#define USB_ENDPOINT_NUMBER_MASK 0x0f /* in endpoint_address */
#define USB_ENDPOINT_DIR_MASK 0x80

#define USB_ENDPOINT_XFERTYPE_MASK 0x03 /* in attributes */
#define USB_ENDPOINT_XFER_CTL 0
#define USB_ENDPOINT_XFER_ISOC 1
#define USB_ENDPOINT_XFER_BULK 2
#define USB_ENDPOINT_XFER_INT 3

typedef struct usb_endpoint_descriptor {
    usb_descriptor_hdr_t hdr;
    uint8_t endpoint_address;
    uint8_t attributes;
    uint16_t max_packet_size;
    uint8_t interval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

/*
 * Buffer to store endpoint descriptors for a specific interface
 *
 * Stored in a linked list
 */
typedef struct usb_endpoint_buffer {
    usb_endpoint_descriptor_t desc;

    struct usb_endpoint_buffer* next;
} usb_endpoint_buffer_t;

static inline bool usb_endpoint_type_is_int(usb_endpoint_buffer_t* ep)
{
    return ((ep->desc.attributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT);
}

static inline bool usb_endpoint_type_is_ctl(usb_endpoint_buffer_t* ep)
{
    return ((ep->desc.attributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_CTL);
}

static inline bool usb_endpoint_type_is_bulk(usb_endpoint_buffer_t* ep)
{
    return ((ep->desc.attributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK);
}

static inline bool usb_endpoint_type_is_isoc(usb_endpoint_buffer_t* ep)
{
    return ((ep->desc.attributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_ISOC);
}

static inline bool usb_endpoint_dir_is_inc(usb_endpoint_buffer_t* ep)
{
    return ((ep->desc.endpoint_address & USB_ENDPOINT_DIR_MASK) == USB_DIRECTION_IN);
}

static inline bool usb_endpoint_dir_is_out(usb_endpoint_buffer_t* ep)
{
    return ((ep->desc.endpoint_address & USB_ENDPOINT_DIR_MASK) == USB_DIRECTION_OUT);
}

typedef struct usb_string_descriptor {
    usb_descriptor_hdr_t hdr;
    char string[];
} __attribute__((packed)) usb_string_descriptor_t;

typedef struct usb_hub_descriptor {
    usb_descriptor_hdr_t hdr;
    uint8_t portcount;
    uint16_t characteristics;
    uint8_t power_stabilize_delay_2ms;
    uint8_t max_power_mA;
    uint8_t removeable;
    uint8_t power_ctl_mask;
} __attribute__((packed)) usb_hub_descriptor_t;
/*
 * Port status
 */
#define USB_PORT_STATUS_CONNECTION 0x0001
#define USB_PORT_STATUS_ENABLE 0x0002
#define USB_PORT_STATUS_SUSPEND 0x0004
#define USB_PORT_STATUS_OVER_CURRENT 0x0008
#define USB_PORT_STATUS_RESET 0x0010
#define USB_PORT_STATUS_L1 0x0020
#define USB_PORT_STATUS_POWER 0x0100
#define USB_PORT_STATUS_LOW_SPEED 0x0200
#define USB_PORT_STATUS_HIGH_SPEED 0x0400
#define USB_PORT_STATUS_TEST 0x0800
#define USB_PORT_STATUS_INDICATOR 0x1000

#define USB_PORT_STATUS_SS_LINK_STATE 0x01e0
#define USB_PORT_STATUS_SS_POWER 0x0200
#define USB_PORT_STATUS_SS_SPEED 0x1c00

typedef struct usb_port_status {
    uint16_t status;
    uint16_t change;
} usb_port_status_t;

/* USB Feature requests */
#define USB_FEATURE_PORT_CONNECTION 0
#define USB_FEATURE_PORT_ENABLE 1
#define USB_FEATURE_PORT_SUSPEND 2
#define USB_FEATURE_PORT_OVER_CURRENT 3
#define USB_FEATURE_PORT_RESET 4
#define USB_FEATURE_PORT_POWER 8
#define USB_FEATURE_PORT_LOW_SPEED 9
#define USB_FEATURE_C_PORT_CONNECTION 16
#define USB_FEATURE_C_PORT_ENABLE 17
#define USB_FEATURE_C_PORT_SUSPEND 18
#define USB_FEATURE_C_PORT_OVER_CURRENT 19
#define USB_FEATURE_C_PORT_RESET 20

#endif // !__ANIVA_USB_SPECIFICATION__
