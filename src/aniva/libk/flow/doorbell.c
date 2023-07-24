#include "doorbell.h"
#include "libk/flow/error.h"
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

  db->m_size = db_size;
  db->m_door_capacity = capacity;
  db->m_flags = flags;
  db->m_door_count = NULL;

  memset(db->m_doors, 0, db->m_size);

  return db;
}

void destroy_doorbell(kdoorbell_t* db)
{
  kfree(db);
}

void doorbell_ring(kdoorbell_t* db)
{
  uint32_t ring_count;
  kdoor_t* current;

  if (!db || !db->m_door_count)
    return;

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
}

void doorbell_reset(kdoorbell_t* db)
{
  uint32_t ring_count;
  kdoor_t* current;

  if (!db)
    return;

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
}

void init_kdoor(kdoor_t* door, void* buffer, uint32_t buffer_size)
{

  if (!door)
    return;

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

ErrorOrPtr register_kdoor(kdoorbell_t* db, kdoor_t* door)
{
  if (!db || !door || door->m_bell)
    return Error();

  if (db->m_door_count == db->m_door_capacity)
    return Error();

  for (uint32_t i = 0; i < db->m_door_capacity; i++) {
    if (!db->m_doors[i]) {
      db->m_doors[i] = door;
      db->m_door_count++;

      door->m_bell = db;
      door->m_idx = i;

      break;
    }
  }

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

  db->m_doors[door->m_idx] = NULL;
  db->m_door_count--;

  door->m_bell = nullptr;
  door->m_idx = KDOOR_INVALID_IDX;

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
