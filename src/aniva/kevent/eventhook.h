#ifndef __ANIVA_EVENTHOOK__
#define __ANIVA_EVENTHOOK__

#include "kevent/context.h"
#include "libk/flow/error.h"

typedef uint32_t (*kevent_hook_fn_t) (kevent_contex_t);
typedef bool (*kevent_hook_condition_fn_t) (kevent_contex_t);

/*
 * NOTE: the kevent privileges are sorted here from
 * highest privilege to lowest privilige, with highest 
 * privilige being zero. This way we can easily loop over
 * all the privileges with one for-loop
 */
enum kevent_privilege {
  KEVENT_PRIVILEGE_SUPERVISOR = 0,
  KEVENT_PRIVILEGE_HIGH,
  KEVENT_PRIVILEGE_MEDIUM,
  KEVENT_PRIVILEGE_LOW,
  KEVENT_PRIVILEGE_LOWEST,

  KEVENT_PRIVILEGE_COUNT = (KEVENT_PRIVILEGE_LOWEST+1)
};

/*
 * The hook into the event
 */
typedef struct kevent_hook {
  enum kevent_privilege m_privilege;

  /* This id is an index into the array that these hooks live in */
  uint32_t m_id;

  /* Function of this hook. Has to be kernel-owned */
  kevent_hook_fn_t m_function;
  /* Condition for this hook to be executed */
  kevent_hook_condition_fn_t m_hook_condition;

  /* Next hook */
  struct kevent_hook* m_next;
} kevent_hook_t;

void init_eventhooks();

kevent_hook_t* create_keventhook(kevent_hook_fn_t function, kevent_hook_condition_fn_t condition, enum kevent_privilege priv);
void destroy_keventhook(kevent_hook_t* hook);

void await_eventhook_fire(uint32_t id);

/*
 * Returns the ID of the hook
 */
int kevent_do_hook(char* e_name, kevent_hook_fn_t function, kevent_hook_condition_fn_t condition, enum kevent_privilege priv);
int kevent_do_unhook(char* e_name, uint32_t hook_id);

int kevent_hook_rehook(uint32_t hook_id, char* old_eventname, char* new_eventname);

int kevent_hook_set_privilege(uint32_t hook_id, enum kevent_privilege priv);

#endif // !__ANIVA_EVENTHOOK__
