#ifndef __ANIVA_EVENTREGISTER__
#define __ANIVA_EVENTREGISTER__
#include "libk/kevent/core.h"
#include "libk/linkedlist.h"

#define ANIVA_EVENT_REGISTRY_NAME "Aniva Event Registry"

struct Processor;

typedef uint32_t event_priority_t;

typedef enum EVENT_PRIORITY {
  EV_PR_LOWEST = 0,
  EV_PR_LOW,
  EV_PR_MID,
  EV_PR_HIGH,
  EV_PR_URGENT,
  EV_PR_DEADLY
} EVENT_PRIORITY_t;

typedef struct event {
  EVENT_TYPE m_type;
  FuncPtr m_callback;
  EVENT_PRIORITY_t m_priority;
} event_t;

typedef struct event_registry {
  list_t* m_registry;
  struct Processor* m_processor;

  char m_registry_name[32];
  bool m_should_disable_interrupts; // wether we allow interrupts to interrupt event calls, TODO: this should prob be per eventcall
} event_registry_t;

void init_event_registry();

event_registry_t* create_registry(const char* name, bool kernel_registry);

ANIVA_STATUS destroy_registry(event_registry_t* registry);

ANIVA_STATUS destroy_registry_by_name(const char* name);

list_t* get_registered_kevents_by_type(EVENT_TYPE type);

ANIVA_STATUS register_event(event_registry_t* registry, EVENT_TYPE type, FuncPtr callback, EVENT_PRIORITY_t prio);

ANIVA_STATUS register_global_kevent(EVENT_TYPE type, FuncPtr callback);

#endif
