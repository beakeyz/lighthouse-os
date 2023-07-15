#include "packet_response.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "mem/zalloc.h"
#include "proc/ipc/tspckt.h"
#include <libk/string.h>

zone_allocator_t* __response_allocator;

packet_response_t* create_packet_response(void* data, size_t size) {
  packet_response_t* response;

  if (!data || !size)
    return NULL;

  response = zalloc_fixed(__response_allocator);

  if (!response)
    return nullptr;

  /* Allocate buffer */
  response->m_response_size = size;
  response->m_response_buffer = kmalloc(size);

  /* Copy data */
  memcpy(response->m_response_buffer, data, size);

  return response;
}

void destroy_packet_response(packet_response_t* response) {
  if (!response) {
    return;
  }

  /* Sanity */
  if (response->m_response_buffer) {
    memset(response->m_response_buffer, 0, response->m_response_size);

    kfree(response->m_response_buffer);
  }

  zfree_fixed(__response_allocator, response);
}

void init_packet_response()
{
  __response_allocator = create_zone_allocator(16 * Kib, sizeof(packet_response_t), NULL);

  ASSERT_MSG(__response_allocator, "Failed to create packet_response allocator");
}
