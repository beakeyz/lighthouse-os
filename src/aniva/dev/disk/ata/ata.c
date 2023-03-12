#include "ata.h"
#include "dev/core.h"

void ata_driver_init();
int ata_driver_exit();

uintptr_t ata_driver_on_packet(packet_payload_t payload, packet_response_t** response);

const aniva_driver_t g_base_ata_driver = {
  .m_name = "ata",
  .m_type = DT_DISK,
  .m_ident = DRIVER_IDENT(0, 0),
  .m_version = DRIVER_VERSION(0, 0, 1),
  .f_init = ata_driver_init,
  .f_exit = ata_driver_exit,
  .f_drv_msg = ata_driver_on_packet,
  .m_port = 6,
  .m_dependencies = {},
  .m_dep_count = 0,
};

void ata_driver_init() {

}

int ata_driver_exit() {
  return 0;
}

uintptr_t ata_driver_on_packet(packet_payload_t payload, packet_response_t** response) {
  return 0;
}
