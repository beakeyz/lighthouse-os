#include "dev/usb/spec.h"
#include <dev/usb/usb.h>

static usb_device_descriptor_t _roothub_descriptor = {
  .length = 18,
  .type = USB_DT_DEVICE,
  .bcd_usb = 0x300,
  .dev_class = 9,
  .dev_subclass = 0,
  .dev_prot = 3,
  .max_pckt_size = 9,
  .vendor_id = 0,
  .product_id = 0,
  .bcd_device = 0x300,
  .product = 0,
  .serial_num = 0,
  .config_count = 0,
};
