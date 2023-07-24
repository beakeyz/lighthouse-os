#ifndef __ANIVA_DOORBELL__
#define __ANIVA_DOORBELL__

/*
 * Kernel utility: kernel doors and doorbells
 *
 * Provide a simple API to signal data to multiple points
 */

#include "libk/flow/error.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct kdoorbell;
struct kdoor;

#define KDOORBELL_MAX_DOOR_COUNT (0xFF)
#define KDOORBELL_FLAG_BUFFERLESS (0x0001) /* We don't use doors their buffers */

typedef struct kdoorbell {
  uint16_t m_flags;
  uint8_t m_door_count;
  uint8_t m_door_capacity;
  uint32_t m_size;

  struct kdoor* m_doors[];
} kdoorbell_t;

kdoorbell_t* create_doorbell(uint8_t capacity, uint16_t flags);
void destroy_doorbell(kdoorbell_t* db);

void doorbell_ring(kdoorbell_t* db);
void doorbell_reset(kdoorbell_t* db);

/*
 * Read from a door their buffer
 * Our buffer should be able to hold the entire doors data, otherwise we bail
 * we return the amount of bytes read
 */
int doorbell_read(kdoorbell_t* db, void* buffer, size_t buffer_size, uint32_t index);

int doorbell_write(kdoorbell_t* db, void* buffer, size_t buffer_size, uint32_t index);
int doorbell_write_c(kdoorbell_t* db, void* buffer, size_t buffer_size, uint32_t start_index, uint32_t count);

#define KDOOR_FLAG_RANG     (0x00000001) /* Has our bell signaled? */
#define KDOOR_FLAG_USER     (0x00000002) /* Do we point into userland? */
#define KDOOR_FLAG_DIRTY    (0x00000004) /* Do we have a dirty buffer? */

#define KDOOR_INVALID_IDX ((uintptr_t)-1)

typedef struct kdoor {
  uint32_t m_flags;
  uint32_t m_buffer_size;
  uintptr_t m_idx;
  mutex_t* m_lock;
  void* m_buffer;
  kdoorbell_t* m_bell;
} kdoor_t;

void init_kdoor(kdoor_t* door, void* buffer, uint32_t buffer_size);
void destroy_kdoor(kdoor_t* door);

ErrorOrPtr register_kdoor(kdoorbell_t* db, kdoor_t* door);
ErrorOrPtr unregister_kdoor(kdoorbell_t* db, kdoor_t* door);
ErrorOrPtr kdoor_move(kdoor_t* door, kdoorbell_t* new_doorbell);

static inline bool kdoor_is_rang(kdoor_t* door)
{
  return (door->m_flags & KDOOR_FLAG_RANG) == KDOOR_FLAG_RANG;
}

#endif // !__ANIVA_DOORBELL__
