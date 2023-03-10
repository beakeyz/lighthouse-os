#include "kterm.h"
#include "dev/debug/serial.h"

void kterm_init();
int kterm_exit();
uintptr_t kterm_on_packet(packet_payload_t payload, packet_response_t** response);

aniva_driver_t g_base_kterm_driver = {
  .m_name = "kterm",
  .m_type = DT_GRAPHICS,
  .f_init = kterm_init,
  .f_exit = kterm_exit,
  .f_drv_msg = kterm_on_packet,
  .m_port = 4,
  .m_dependencies = {"graphics.fb"},
  .m_dep_count = 1,
};

void kterm_init() {
  println("Initialized the kterm!");
}

int kterm_exit() {
  return 0;
}

uintptr_t kterm_on_packet(packet_payload_t payload, packet_response_t** response) {
  return 0;
}
