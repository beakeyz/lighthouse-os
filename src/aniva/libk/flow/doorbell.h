#ifndef __ANIVA_DOORBELL__
#define __ANIVA_DOORBELL__

/*
 * Kernel utility: kernel doors and doorbells
 *
 * Provide a simple API to signal data to multiple points
 *
 * TODO: Transfer this functionality to kevents
 */

#include "libk/flow/error.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

struct kdoorbell;
struct kdoor;

#define KDOORBELL_MAX_DOOR_COUNT (0xFF)
#define KDOORBELL_FLAG_BUFFERLESS (0x0001) /* We don't use doors their buffers */

/*
 * Kernel Doorbell
 *
 * Kernel doorbells are structures that act as notifiers for certain events.
 * Every doorbell can have a maximum of KDOORBELL_MAX_DOOR_COUNT doors attached to
 * it which will listen to the doorbell for notifications (rings). When a ring happens
 * the doorbell will go though all its doors and go through the ringing procedure
 * per door. This consists of:
 * - Locking the door mutex
 * - Checking if the door has a buffer
 *  - If yes, write the appropriate data to the buffer
 * - Flip the KDOOR_FLAG_RANG bit in the doors flags
 * - Unlock the door
 */
typedef struct kdoorbell {
    uint16_t m_flags;
    uint8_t m_door_count;
    uint8_t m_door_capacity;
    uint32_t m_size;

    mutex_t* m_lock;

    struct kdoor* m_doors[];
} kdoorbell_t;

kdoorbell_t* create_doorbell(uint8_t capacity, uint16_t flags);
void destroy_doorbell(kdoorbell_t* db);

void doorbell_ring(kdoorbell_t* db);
void doorbell_reset(kdoorbell_t* db);

void doorbell_ring_one(kdoorbell_t* db, uint32_t idx);

static inline bool doorbell_has_door(kdoorbell_t* db, uint32_t idx)
{
    return (idx < db->m_door_capacity && db->m_doors[idx] != nullptr);
}

/*
 * Read from a door their buffer
 * Our buffer should be able to hold the entire doors data, otherwise we bail
 * we return the amount of bytes read
 */
int doorbell_read(kdoorbell_t* db, void* buffer, size_t buffer_size, uint32_t index);

int doorbell_write(kdoorbell_t* db, void* buffer, size_t buffer_size, uint32_t index);
int doorbell_write_c(kdoorbell_t* db, void* buffer, size_t buffer_size, uint32_t start_index, uint32_t count);

#define KDOOR_FLAG_RANG (0x00000001) /* Has our bell signaled? */
#define KDOOR_FLAG_USER (0x00000002) /* Do we point into userland? */
#define KDOOR_FLAG_DIRTY (0x00000004) /* Do we have a dirty buffer? */

#define KDOOR_INVALID_IDX ((uintptr_t)-1)

/*
 * A kernel door
 *
 * This is basically a waiter for a kernel event in the form of a doorbell. Every door
 * is attached to a doorbell that can ring it to notify an event. In order to provide
 * a finer control over the event types we provide a buffer per door that can hold private
 * information that depends on the type doorbell
 */
typedef struct kdoor {
    kdoorbell_t* m_bell;

    uint32_t m_flags;
    uintptr_t m_idx;

    mutex_t* m_lock;

    uint32_t m_buffer_size;
    void* m_buffer;
} kdoor_t;

void init_kdoor(kdoor_t* door, void* buffer, uint32_t buffer_size);
void destroy_kdoor(kdoor_t* door);

kerror_t register_kdoor(kdoorbell_t* db, kdoor_t* door);
kerror_t unregister_kdoor(kdoorbell_t* db, kdoor_t* door);
kerror_t kdoor_move(kdoor_t* door, kdoorbell_t* new_doorbell);

kerror_t kdoor_reset(kdoor_t* door);

static inline bool kdoor_is_rang(kdoor_t* door)
{
    /* If (somehow) we end up without a bell, consider ourself rang */
    if (!door->m_bell)
        return true;

    return ((door->m_flags & KDOOR_FLAG_RANG) == KDOOR_FLAG_RANG);
}

static inline bool kdoor_is_attached(kdoor_t* door)
{
    return (door->m_bell != nullptr);
}

#endif // !__ANIVA_DOORBELL__
