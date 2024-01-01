#ifndef __LIGHTENV_LIBSYS_IPC_SOCKET__
#define __LIGHTENV_LIBSYS_IPC_SOCKET__

#include "def.h"

/*!
 * @brief: Try to initiate a socket connection to a target process
 *
 */
int create_socket(ipc_socket_t** p_socket, HANDLE target);

/*!
 * @brief: Destroy a socket connection
 */
int destroy_socket(ipc_socket_t* socket);

int socket_get_free_packet(ipc_socket_t* socket, ipc_packet_t** packet);
int init_ipc_packet(ipc_packet_t* packet, uint8_t* data, uint32_t size);

struct ipc_packet* socket_probe(ipc_socket_t* socket);
int socket_transfer(ipc_socket_t*, ipc_packet_t* packet);

#endif // !__LIGHTENV_LIBSYS_IPC_SOCKET__
