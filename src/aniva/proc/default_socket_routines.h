#ifndef __ANIVA_PROC_DEFAULT_SOCKET_ROUTINE__
#define __ANIVA_PROC_DEFAULT_SOCKET_ROUTINE__
#include "libk/error.h"
#include <libk/stddef.h>

#define SOCKET_ROUTINE_IDENT 0x69

#define SOCKET_ROUTINE_EXIT 0x00
#define SOCKET_ROUTINE_IGNORE_NEXT 0x01
#define SOCKET_ROUTINE_DEBUG 0x02

void send_socket_routine(uint32_t port, uint8_t routine);

ANIVA_STATUS parse_socket_routing(uint16_t msg, uint8_t* ident, uint8_t* routine);

#endif // !__ANIVA_PROC_DEFAULT_SOCKET_ROUTINE__
