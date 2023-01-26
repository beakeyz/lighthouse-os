#ifndef __ANIVA_SOCKET_THREAD__
#define __ANIVA_SOCKET_THREAD__
#include <libk/stddef.h>
#include <libk/error.h>

#define MIN_SOCKET_BUFFER_SIZE 0
#define DEFAULT_SOCKET_BUFFER_SIZE 4096

typedef enum THREADED_SOCKET_STATE {
  THREADED_SOCKET_STATE_LISTENING = 0,
  THREADED_SOCKET_STATE_BUSY,
  THREADED_SOCKET_STATE_BLOCKED
} THREADED_SOCKET_STATE_t;

typedef struct threaded_socket {
  uint32_t m_port;
  size_t m_buffer_size;
  void* m_buffer;
  THREADED_SOCKET_STATE_t m_state;
} threaded_socket_t;

typedef struct ts_packet {
  uint32_t m_identifier; // checksum? (like some form of security in this system lmao)
  void* m_data;
} ts_packet_t;

threaded_socket_t *create_threaded_socket(uint32_t port, size_t buffer_size);
ANIVA_STATUS destroy_threaded_socket(threaded_socket_t* ptr);

ANIVA_STATUS send_packet_to_socket(uint32_t port, void* buffer);

#endif

