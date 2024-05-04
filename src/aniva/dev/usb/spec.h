#ifndef __ANIVA_USB_SPECIFICATION__
#define __ANIVA_USB_SPECIFICATION__

#include <libk/stddef.h>

/*
 * Standard requests, for the bRequest field of a SETUP packet.
 *
 * These are qualified by the bRequestType field, so that for example
 * TYPE_CLASS or TYPE_VENDOR specific feature flags could be retrieved
 * by a GET_STATUS request.
 *
 * Yoinked from: https://github.com/torvalds/linux/blob/master/include/uapi/linux/usb/ch9.h
 */
#define USB_REQ_GET_STATUS		0x00
#define USB_REQ_CLEAR_FEATURE		0x01
#define USB_REQ_SET_FEATURE		0x03
#define USB_REQ_SET_ADDRESS		0x05
#define USB_REQ_GET_DESCRIPTOR		0x06
#define USB_REQ_SET_DESCRIPTOR		0x07
#define USB_REQ_GET_CONFIGURATION	0x08
#define USB_REQ_SET_CONFIGURATION	0x09
#define USB_REQ_GET_INTERFACE		0x0A
#define USB_REQ_SET_INTERFACE		0x0B
#define USB_REQ_SYNCH_FRAME		0x0C
#define USB_REQ_SET_SEL			0x30
#define USB_REQ_SET_ISOCH_DELAY		0x31
#define USB_REQ_SET_ENCRYPTION		0x0D	/* Wireless USB */
#define USB_REQ_GET_ENCRYPTION		0x0E
#define USB_REQ_RPIPE_ABORT		0x0E
#define USB_REQ_SET_HANDSHAKE		0x0F
#define USB_REQ_RPIPE_RESET		0x0F
#define USB_REQ_GET_HANDSHAKE		0x10
#define USB_REQ_SET_CONNECTION		0x11
#define USB_REQ_SET_SECURITY_DATA	0x12
#define USB_REQ_GET_SECURITY_DATA	0x13
#define USB_REQ_SET_WUSB_DATA		0x14
#define USB_REQ_LOOPBACK_DATA_WRITE	0x15
#define USB_REQ_LOOPBACK_DATA_READ	0x16
#define USB_REQ_SET_INTERFACE_DS	0x17

/* specific requests for USB Power Delivery */
#define USB_REQ_GET_PARTNER_PDO		20
#define USB_REQ_GET_BATTERY_STATUS	21
#define USB_REQ_SET_PDO			22
#define USB_REQ_GET_VDM			23
#define USB_REQ_SEND_VDM		24

#define USB_TYPE_DEV_IN         (0x80)
#define USB_TYPE_DEV_OUT        (0x00)
#define USB_TYPE_IF_IN          (0x81)
#define USB_TYPE_IF_OUT         (0x01)
#define USB_TYPE_EP_IN          (0x82)
#define USB_TYPE_EP_OUT         (0x02)
#define USB_TYPE_OTHER_IN       (0x83)
#define USB_TYPE_OTHER_OUT      (0x03)

#define USB_TYPE_MASK			(0x03 << 5)
#define USB_TYPE_STANDARD		(0x00 << 5)
#define USB_TYPE_CLASS			(0x01 << 5)
#define USB_TYPE_VENDOR			(0x02 << 5)
#define USB_TYPE_RESERVED		(0x03 << 5)

/*
 * Simple structure of a usb control request
 */
typedef struct usb_ctlreq {
  uint8_t request_type;
  uint8_t request;
  uint16_t value;
  uint16_t index;
  uint16_t length;
} __attribute__ ((packed)) usb_ctlreq_t;

/*
 * USB Descriptor types
 */
#define USB_DT_DEVICE 0x01
#define USB_DT_CONFIG 0x02
#define USB_DT_STRING 0x03
#define USB_DT_INTERFACE 0x04
#define USB_DT_ENDPOINT	0x05
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
#define USB_DT_WIRE_ADAPTER	0x21
#define USB_DT_RPIPE 0x22
#define USB_DT_CS_RADIO_CONTROL	0x23
#define USB_DT_PIPE_USAGE 0x24
#define	USB_DT_SS_ENDPOINT_COMP	0x30
#define	USB_DT_SSP_ISOC_ENDPOINT_COMP 0x31

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

/*
 * Port status
 */
#define USB_PORT_STATUS_CONNECTION		0x0001
#define USB_PORT_STATUS_ENABLE			0x0002
#define USB_PORT_STATUS_SUSPEND			0x0004
#define USB_PORT_STATUS_OVER_CURRENT	0x0008
#define USB_PORT_STATUS_RESET			0x0010
#define USB_PORT_STATUS_L1				0x0020
#define USB_PORT_STATUS_POWER			0x0100
#define USB_PORT_STATUS_LOW_SPEED		0x0200
#define USB_PORT_STATUS_HIGH_SPEED		0x0400
#define USB_PORT_STATUS_TEST			0x0800
#define USB_PORT_STATUS_INDICATOR		0x1000

#define USB_PORT_STATUS_SS_LINK_STATE	0x01e0
#define USB_PORT_STATUS_SS_POWER		0x0200
#define USB_PORT_STATUS_SS_SPEED		0x1c00

typedef struct usb_port_status {
  uint16_t status;
  uint16_t change;
} usb_port_status_t;

/* USB Feature requests */
#define USB_FEATURE_PORT_CONNECTION				0
#define USB_FEATURE_PORT_ENABLE					1
#define USB_FEATURE_PORT_SUSPEND				2
#define USB_FEATURE_PORT_OVER_CURRENT			3
#define USB_FEATURE_PORT_RESET					4
#define USB_FEATURE_PORT_POWER					8
#define USB_FEATURE_PORT_LOW_SPEED				9
#define USB_FEATURE_C_PORT_CONNECTION			16
#define USB_FEATURE_C_PORT_ENABLE				17
#define USB_FEATURE_C_PORT_SUSPEND				18
#define USB_FEATURE_C_PORT_OVER_CURRENT			19
#define USB_FEATURE_C_PORT_RESET				20

#endif // !__ANIVA_USB_SPECIFICATION__
