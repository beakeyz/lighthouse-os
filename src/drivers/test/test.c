#include "dev/debug/serial.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include <dev/core.h>
#include <dev/driver.h>
#include <libk/stddef.h>

int test_init() {
  return 0;
}

int test_exit() {

  return 0;
}

uintptr_t test_msg(packet_payload_t payload, packet_response_t** response) {

  return 0;
}

aniva_driver_t extern_test_driver = {
  .m_name = "ExternalTest",
  .m_descriptor = "Just funnie test",
  .m_version = DRIVER_VERSION(0, 0, 1),
  .m_ident = DRIVER_IDENT(0, 0),
  .m_type = DT_OTHER,
  .f_init = test_init,
  .f_exit = test_exit,
  .f_drv_msg = test_msg,
  .m_dep_count = 0,
};

EXPORT_DRIVER(extern_test_driver);
