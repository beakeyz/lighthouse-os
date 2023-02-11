#include "eventregister.h"
#include "dev/debug/serial.h"
#include "libk/kevent/core.h"
#include "libk/kevent/eventhook.h"
#include "libk/linkedlist.h"
#include "libk/stddef.h"
#include <mem/heap.h>
#include "system/processor/processor.h"
#include <libk/error.h>
#include <libk/string.h>

static event_registry_t* s_kevent_registry = nullptr;

// --- TEST FUNC ---
void __test_event_handler(struct time_update_event_hook* hook) {
  println("hi from __test_event_handler");
  hook->ticks++;
}

void init_global_kevents() { // from "libk/kevent/core.h"
  init_event_registry();
}

event_registry_t* create_registry(const char* name, bool kernel_registry) {
  event_registry_t* reg = kmalloc(sizeof(event_registry_t));

  // TODO: move into function?
  {
    const char* ivn = "Invalid_name";
    strcpy((char*)&reg->m_registry_name[0], ivn);

    if (strlen(name) <= 32) {
      strcpy((char*)&reg->m_registry_name[0], name);
    }
  }

  reg->m_processor = get_current_processor();

  reg->m_registry = init_list();

  // NOTE: for now we rank kernel event higher than system interrupts (excluding exeptions).
  // so we only let interrupts interfere when this registry is not meant for kernel shit
  reg->m_should_disable_interrupts = kernel_registry;

  return reg;
}

ANIVA_STATUS register_event(event_registry_t* registry, EVENT_TYPE type, FuncPtr callback, EVENT_PRIORITY_t prio) {
  ASSERT(registry != nullptr);

  // TODO: check if given type matches callback type

  struct event* event = kmalloc(sizeof(struct event));
  event->m_callback = callback;
  event->m_type = type;
  event->m_priority = prio;

  list_append(registry->m_registry, event);
  return ANIVA_SUCCESS;
}

// Kernel events are of high priority
ANIVA_STATUS register_global_kevent(EVENT_TYPE type, FuncPtr callback) {
  return register_event(s_kevent_registry, type, callback, EV_PR_HIGH);
}

void init_event_registry() {
  if (s_kevent_registry != nullptr) {
    // TODO: cleanup
  }

  s_kevent_registry = create_registry(ANIVA_EVENT_REGISTRY_NAME, true);
}

// TODO:
ANIVA_STATUS destroy_registry(event_registry_t* registry) {
  return ANIVA_FAIL;
}

// TODO:
ANIVA_STATUS destroy_registry_by_name(const char* name) {
  return ANIVA_FAIL;
}

list_t* get_registered_kevents_by_type(EVENT_TYPE type) {
  list_t* ret = init_list();

  FOREACH(n, s_kevent_registry->m_registry) {
    struct event* event = (struct event*)n->data;

    if (event->m_type == type) {
      list_append(ret, event);
    }
  }

  if (ret->m_length == 0) {
    kfree(ret);
    return nullptr;
  }

  return ret;
}
