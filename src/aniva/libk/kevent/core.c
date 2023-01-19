#include "core.h"
#include <libk/error.h>
#include "eventhook.h"
#include "libk/kevent/eventregister.h"
#include "libk/linkedlist.h"
#include "mem/kmalloc.h"

ALWAYS_INLINE void clean_temp_event_list(list_t* list) {
  uintptr_t i = 0;
  void* ptrs[list->m_length];
  FOREACH(ptr, list) {
    ptrs[i] = ptr;
    i++;
  }

  for (uintptr_t j = 0; j < list->m_length; j++) {
    kfree(ptrs[j]);
  }
  kfree(list);
}

ANIVA_STATUS call_event(EVENT_TYPE type, void* hook) {

  ASSERT(hook != nullptr);

  // we love macros =DDDDDDDDDDDDDDDDDD
#define CALL_EVENTS(__Hook, __Callback)                                             \
      __Hook* tue_h = (__Hook*)hook;                                                \
      list_t* events = get_registered_kevents_by_type(type);                        \
      if (events == nullptr) {                                                      \
        return ANIVA_FAIL;                                                          \
      }                                                                             \
      FOREACH(node, events) {                                                       \
        struct event* ev = (struct event*)node->data;                               \
        __Callback callback = (__Callback)ev->m_callback;                           \
        callback(tue_h);                                                            \
      }                                                                             \
      clean_temp_event_list(events)

  switch (type) {
    case TIME_UPDATE_EVENT: {
      CALL_EVENTS(struct time_update_event_hook, TU_EVENT_CALLBACK);
      break;
    }
    case SCHEDULER_ENTRY_EVENT: {
      CALL_EVENTS(struct sched_enter_event_hook, S_EN_EVENT_CALLBACK);
      break;
    }
    case SCHEDULER_EXIT_EVENT: {
      CALL_EVENTS(struct sched_exit_event_hook, S_EX_EVENT_CALLBACK);
      break;
    }
    case THREAD_SIGNAL_EVENT: {
      CALL_EVENTS(struct thread_signal_event_hook, T_SIG_EVENT_CALLBACK);
      break;
    }
  }

#undef CALL_EVENTS

  return ANIVA_SUCCESS;
}

struct time_update_event_hook create_time_update_event_hook(size_t ticks) {
  struct time_update_event_hook ret = {
    .ticks = ticks
  };
  return ret;
}
struct sched_enter_event_hook create_sched_enter_event_hook() {
  struct sched_enter_event_hook ret = {
    
  };
  return ret;
}
struct sched_exit_event_hook create_sched_exit_event_hook() {
  struct sched_exit_event_hook ret = {

  };
  return ret;
}
struct thread_signal_event_hook create_thread_signal_event_hook(struct thread* thread) {
  struct thread_signal_event_hook ret = {
    .thread = thread,
  };
  return ret;
}
