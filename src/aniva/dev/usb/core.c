#include "dev/driver.h"

int usb_generic_init();

/*
 * Main entrypoint for any USB interractions
 */
aniva_driver_t usb_generic = {
  .m_name = "usb_generic",
  .f_init = usb_generic_init,
};

int usb_generic_init()
{
  return 0;
}
