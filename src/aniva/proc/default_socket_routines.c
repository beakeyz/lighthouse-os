#include "default_socket_routines.h"
#include "proc/core.h"
#include "proc/socket.h"

void send_socket_routine(uint32_t port, uint8_t routine) {

  uint16_t buffer = ((SOCKET_ROUTINE_IDENT << 8) & 0xFFFF) | routine;
  
  send_packet_to_socket(port, &buffer, sizeof(buffer));
}

ANIVA_STATUS parse_socket_routing(uint16_t msg, uint8_t* ident, uint8_t* routine) {

  uint8_t _ident = (msg >> 8) & 0xFF;
  uint8_t _routine = (uint8_t)(msg) & 0xFF;

  if (_ident == SOCKET_ROUTINE_IDENT) {
    *ident = _ident;
    *routine = _routine;
    return ANIVA_SUCCESS;
  }
  *ident = 0;
  *routine = 0;
  return ANIVA_FAIL;
}
