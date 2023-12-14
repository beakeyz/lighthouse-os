#ifndef __ANIVA_KEVENT__
#define __ANIVA_KEVENT__

#include "eventhook.h"
#include "context.h"
#include "libk/flow/error.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include <libk/stddef.h>

#define KEVENT_NAME_MAX_LENGTH 32

#define KEVENT_MAX_EVENTS           (256)
#define KEVENT_MAX_EVENT_HOOKS      (1024)

// #define KEVENT_FLAG_NAMELESS            (0x00000001) /* This event is identified only by its ID and type */
#define KEVENT_FLAG_NO_CHAINING         (0x00000001) /* This event only holds one handler */
#define KEVENT_FLAG_HIGH_PRIO           (0x00000002) /* This event is allowed to interrupt kernel processes like scheduling*/

#define KEVENT_TYPE_COUNT 6

typedef enum kevent_type {
  KEVENT_TYPE_IRQ = 0,
  KEVENT_TYPE_SYSCAL,
  KEVENT_TYPE_PCI,
  KEVENT_TYPE_UNKNOWN,
  KEVENT_TYPE_CUSTOM,
  KEVENT_TYPE_ERROR,
} kevent_type_t;

const char* kevent_type_to_str(kevent_type_t type);
kevent_type_t str_to_kevent_type(const char*);

typedef struct kevent {
  uint32_t m_flags;
  kevent_type_t m_type;

  /*
   * Protects the entire object and should be held when
   * the event is fired
   */
  mutex_t* m_firing_lock;

  char m_name[KEVENT_NAME_MAX_LENGTH];

  zone_allocator_t* m_hook_allocator;

  size_t m_max_hooks_count;
  size_t m_hooks_count;

  /*
   * The way we do this, when we give out pointers to these
   * eventhooks, we also give out pointers to the kevent object,
   * so we pretty much never want to give external systems access to 
   * the address of these hooks, but when they do need to know something,
   * or change something about the hook, we should give them a deep copy,
   * or do mutation based on ID
   */
  kevent_hook_t* m_hooks;

  /* Array the size of m_hooks_count. Recomputed on every kevent_attach_hook */
  kevent_hook_t** m_exec_hooks;
} kevent_t;


/*
 * For every possible event that can happen in the kernel 
 * (for example, interrupts, device hotplugs, syscalls, ect) a kevent should be there handling
 * the event and managing the handlers of the event. as such, we need infrastructure
 * for creating event 'blueprints' that describe how a certain event is triggered and handled and 
 * also for handling the actual events as they happen.
 *
 * We should also think about what kernel processes these events are allowed to interrupt. This goes 
 * for scheduling, I/O blocks and wakes, ect.
 */

void init_kevents();

/*
 * Try to create an event and return its key
 */
int create_kevent(char* name, kevent_type_t type, uint32_t flags, size_t max_hook_count);

/*
 * Try to remove an event
 * fails if the name is not found or if the key is invalid
 */
int destroy_kevent(kevent_t* event);

kevent_t* kevent_get(char* name);
size_t kevent_get_hook_count(char* name);

int kevent_attach_hooks(kevent_t* event, kevent_hook_t** hooks, size_t count);
int kevent_attach_hook(kevent_t* event, kevent_hook_t* hook);
int kevent_detach_hook(kevent_t* event, kevent_hook_t* hook);

/*
 * Try to call the eventlisteners
 * fails if the name is not found or if the key is invalid
 * or if the callchain fails
 */
int fire_event(char* name, void* data, size_t size); 

static inline bool ke_did_fail(int res)
{
  return (res < 0);
}

#endif // !__ANIVA_KEVENT__
