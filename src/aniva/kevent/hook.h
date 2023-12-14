#ifndef __ANIVA_KEVENT_HOOK__ 
#define __ANIVA_KEVENT_HOOK__

#include "kevent/event.h"

typedef int (*f_hook_fn) (kevent_ctx_t* ctx);


/*
 * TODO: Depending on the type of hook that gets created, the hook gets handled differently
 * when the kevent is fired
 */
#define KEVENT_HOOKTYPE_FUNC  0
#define KEVENT_HOOKTYPE_QUEUE 1

/*
 * Memory block that indicates a hook to be called
 *
 */
typedef struct kevent_hook {
  const char* hookname;

  f_hook_fn f_hook;

  bool is_frozen:1;
  bool is_set:1;
  uint8_t type;
  /* Hashed hookname */
  uint32_t key;

  /* Double linking for the traverse */
  struct kevent_hook* next;
  struct kevent_hook* prev;
} kevent_hook_t;

int init_keventhook(kevent_hook_t* hook, const char* name, f_hook_fn hook_fn);
void destroy_keventhook(kevent_hook_t* hook);

int keventhook_call(kevent_hook_t* hook, kevent_ctx_t* ctx);

bool keventhook_is_set(kevent_hook_t* hook);

#endif //__ANIVA_KEVENT_HOOK__
