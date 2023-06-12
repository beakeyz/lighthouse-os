#ifndef __ANIVA_KEVENT__
#define __ANIVA_KEVENT__

#include "eventhook.h"
#include "context.h"
#include "libk/error.h"
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

typedef uint32_t kevent_key_t;

const char* kevent_type_to_str(kevent_type_t type);
kevent_type_t str_to_kevent_type(const char*);

typedef struct kevent {
  uint32_t m_id;
  uint32_t m_flags;
  kevent_type_t m_type;

  /*
   * Protects the entire object and should be held when
   * the event is fired
   */
  spinlock_t* m_firing_lock;

  /*
   * This key is an crc32 of the finished kevent object 
   * and should be used to verify actions on this kevent
   */
  kevent_key_t m_key;

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
ErrorOrPtr create_kevent(char* name, kevent_type_t type, uint32_t flags, size_t max_hook_count);

/*
 * Try to remove an event
 * fails if the name is not found or if the key is invalid
 */
ErrorOrPtr destroy_kevent(char* name, kevent_key_t key);

kevent_t* kevent_get(char* name);
size_t kevent_get_hook_count(char* name);

ErrorOrPtr kevent_attach_hook(kevent_t* event, kevent_hook_t* hook);
ErrorOrPtr kevent_detach_hook(kevent_t* event, kevent_hook_t* hook);

ErrorOrPtr kevent_set_max_hooks(kevent_t** event, size_t new_max_hook_count);
ErrorOrPtr kevent_set_flags(kevent_t** event, uint32_t flags);

/*
 * Try to call the eventlisteners
 * fails if the name is not found or if the key is invalid
 * or if the callchain fails
 */
ErrorOrPtr fire_event(char* name, kevent_key_t key, kevent_contex_t* data); 

#endif // !__ANIVA_KEVENT__