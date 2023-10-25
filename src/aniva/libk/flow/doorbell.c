#include "doorbell.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include <libk/string.h>

kdoorbell_t* create_doorbell(uint8_t capacity, uint16_t flags)
{
  kdoorbell_t* db;
  uint32_t db_size;

  if (!capacity)
    return nullptr;

  db_size = (capacity * sizeof(kdoor_t));

  db = kmalloc(sizeof(kdoorbell_t) + db_size);

  if (!db)
    return nullptr;

  db->m_lock = create_mutex(NULL);
  db->m_size = db_size;
  db->m_door_capacity = capacity;
  db->m_flags = flags;
  db->m_door_count = NULL;

  memset(db->m_doors, 0, db->m_size);

  return db;
}

void destroy_doorbell(kdoorbell_t* db)
{
  kdoor_t* current;

  if (!db)
    return;

  for (uint32_t i = 0; i < db->m_door_capacity; i++) {
    current = db->m_doors[i];

    if (!current || !current->m_lock)
      continue;

    mutex_lock(current->m_lock);

    current->m_bell = nullptr;

    mutex_unlock(current->m_lock);
  }

  destroy_mutex(db->m_lock);
  kfree(db);
}

void doorbell_ring_one(kdoorbell_t* db, uint32_t idx)
{
  kdoor_t* door;

  if (idx >= db->m_door_capacity)
    return;

  door = db->m_doors[idx];

  if (!door)
    return;

  mutex_lock(db->m_lock);
  mutex_lock(door->m_lock);

  door->m_flags |= KDOOR_FLAG_RANG;

  mutex_unlock(door->m_lock);
  mutex_unlock(db->m_lock);
}

void doorbell_ring(kdoorbell_t* db)
{
  uint32_t ring_count;
  kdoor_t* current;

  if (!db || !db->m_door_count)
    return;

  mutex_lock(db->m_lock);

  ring_count = 0;

  for (uint32_t i = 0; i < db->m_door_capacity; i++) {
    current = db->m_doors[i];

    if (current) {

      mutex_lock(current->m_lock);

      current->m_flags |= KDOOR_FLAG_RANG;

      mutex_unlock(current->m_lock);

      ring_count++;

      /* We've got evey door */
      if (ring_count >= db->m_door_count)
        break;
    }
  }

  mutex_unlock(db->m_lock);
}

void doorbell_reset(kdoorbell_t* db)
{
  uint32_t ring_count;
  kdoor_t* current;

  if (!db)
    return;

  mutex_lock(db->m_lock);

  ring_count = 0;

  for (uint32_t i = 0; i < db->m_door_capacity; i++) {
    current = db->m_doors[i];

    if (current) {

      mutex_lock(current->m_lock);

      current->m_flags &= ~KDOOR_FLAG_RANG;

      mutex_unlock(current->m_lock);

      ring_count++;

      /* We've got evey door */
      if (ring_count >= db->m_door_count)
        break;
    }
  }

  mutex_unlock(db->m_lock);
}

/*!
 * @brief Write to the buffer of a specific doorbell without writing to it
 *
 * This returns failure when the door is locked
 */
int doorbell_write(kdoorbell_t* db, void* buffer, size_t buffer_size, uint32_t index)
{
  kdoor_t* door;

  /* No buffers reported by the doorbell, trust this to prevent accidental writes */
  if ((db->m_flags & KDOORBELL_FLAG_BUFFERLESS) == KDOORBELL_FLAG_BUFFERLESS)
    return 0;

  door = db->m_doors[index];

  if (mutex_is_locked(door->m_lock))
    return -1;

  if (!door->m_buffer || !door->m_buffer_size)
    return -1;

  /* Prevent overflows */
  if (buffer_size > door->m_buffer_size)
    return -2;

  mutex_lock(door->m_lock);

  /* All checsk passed. We can safely write to the buffer */
  memcpy(door->m_buffer, buffer, buffer_size);

  mutex_unlock(door->m_lock);
  return 0;
}

/*!
 * @brief Initialize the memory for a kernel door
 *
 * The buffer of the door is managed by the caller
 */
void init_kdoor(kdoor_t* door, void* buffer, uint32_t buffer_size)
{
  if (!door)
    return;

  memset(door, 0, sizeof(*door));

  door->m_flags = NULL;
  door->m_buffer = buffer;
  door->m_buffer_size = buffer_size;
  door->m_bell = nullptr;
  door->m_idx = KDOOR_INVALID_IDX;
  door->m_lock = create_mutex(NULL);
}

void destroy_kdoor(kdoor_t* door)
{
  destroy_mutex(door->m_lock);

  if (door->m_buffer && door->m_buffer_size)
    door->m_flags |= KDOOR_FLAG_DIRTY;
}

ErrorOrPtr kdoor_reset(kdoor_t* door)
{
  kdoorbell_t* bell;

  if (!door->m_bell)
    return Error();

  bell = door->m_bell;

  if (!bell->m_lock)
    return Error();

  mutex_lock(bell->m_lock);
  mutex_lock(door->m_lock);

  door->m_flags &= ~(KDOOR_FLAG_RANG);

  mutex_unlock(door->m_lock);
  mutex_unlock(bell->m_lock);

  return Success(0);
}

ErrorOrPtr register_kdoor(kdoorbell_t* db, kdoor_t* door)
{
  if (!db || !door || door->m_bell)
    return Error();

  if (db->m_door_count == db->m_door_capacity)
    return Error();

  mutex_lock(db->m_lock);

  for (uint32_t i = 0; i < db->m_door_capacity; i++) {
    if (!db->m_doors[i]) {
      db->m_doors[i] = door;
      db->m_door_count++;

      door->m_bell = db;
      door->m_idx = i;

      break;
    }
  }

  mutex_unlock(db->m_lock);
  return Success(0);
}

ErrorOrPtr unregister_kdoor(kdoorbell_t* db, kdoor_t* door)
{
  if (!db || !door || !door->m_bell)
    return Error();

  if (!db->m_door_count || door->m_idx >= db->m_door_capacity)
    return Error();

  if (!db->m_doors[door->m_idx])
    return Error();

  mutex_lock(db->m_lock);

  db->m_doors[door->m_idx] = NULL;
  db->m_door_count--;

  door->m_bell = nullptr;
  door->m_idx = KDOOR_INVALID_IDX;

  mutex_unlock(db->m_lock);
  return Success(0);
}


ErrorOrPtr kdoor_move(kdoor_t* door, kdoorbell_t* new_doorbell)
{
  ErrorOrPtr result;

  result = unregister_kdoor(door->m_bell, door);

  if (IsError(result))
    return result;

  result = register_kdoor(new_doorbell, door);

  if (IsError(result))
    return Error();

  return Success(0);
}
