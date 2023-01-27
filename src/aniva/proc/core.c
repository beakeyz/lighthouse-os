#include "core.h"

vector_t * g_sockets = nullptr;

ANIVA_STATUS initialize_proc_core() {
  g_sockets = create_vector(SOCKET_DEFAULT_MAXIMUM_SOCKET_COUNT);

  // TODO: more???
}