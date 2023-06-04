#ifndef __ANIVA_EVENTHOOK__
#define __ANIVA_EVENTHOOK__

#include "kevent/context.h"
#include "libk/error.h"

typedef kevent_contex_t* (*kevent_hook_fn_t) (kevent_contex_t*);

typedef enum kevent_previlige {
  KEVENT_PRIVILEGE_SUPERVISOR = 0,
  KEVENT_PRIVILEGE_HIGH,
  KEVENT_PRIVILEGE_MEDIUM,
  KEVENT_PRIVILEGE_LOW,
  KEVENT_PRIVILEGE_LOWEST,
} kevent_privilege_t;

typedef struct kevent_hook {
  kevent_privilege_t m_privilege;

  /* This id is an index into the array that these hooks live in */
  uint32_t m_id;

  kevent_hook_fn_t m_function;

  struct kevent_hook* m_next;
} kevent_hook_t;

/*
 * Returns the ID of the hook
 */
ErrorOrPtr kevent_hook(char* e_name, kevent_hook_fn_t function);
ErrorOrPtr kevent_unhook(char* e_name, uint32_t id);

#endif // !__ANIVA_EVENTHOOK__
