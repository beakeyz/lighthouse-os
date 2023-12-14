#include "eventhook.h"
#include "crypto/k_crc32.h"
#include "kevent/context.h"
#include "kevent/kevent.h"
#include "libk/flow/error.h"
#include "mem/zalloc.h"

static zone_allocator_t* __kevent_hook_allocator = nullptr;

static bool _always_hook_condition(kevent_contex_t ctx)
{
  return true;
}

void init_eventhooks() 
{
  /* ALlocator for the hooks */
  __kevent_hook_allocator = create_zone_allocator_ex(nullptr, NULL, sizeof(kevent_hook_t) * KEVENT_MAX_EVENT_HOOKS, sizeof(kevent_hook_t), NULL); 
}

/*!
 * @brief: Allocate a kevent hook
 */
kevent_hook_t* create_keventhook(kevent_hook_fn_t function, kevent_hook_condition_fn_t condition, enum kevent_privilege priv)
{
  kevent_hook_t* ret;

  if (!__kevent_hook_allocator)
    return nullptr;

  ret = zalloc_fixed(__kevent_hook_allocator);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(kevent_hook_t));

  ret->m_function = function;
  ret->m_hook_condition = condition;
  ret->m_privilege = priv;

  ret->m_id = kcrc32(ret, sizeof(kevent_hook_t));

  return ret;
}

void destroy_keventhook(kevent_hook_t* hook) 
{
  zfree_fixed(__kevent_hook_allocator, hook);
}

/*!
 * @brief: Create an event hook and attach it to the kevent
 */
int kevent_do_hook(char* e_name, kevent_hook_fn_t function, kevent_hook_condition_fn_t condition, enum kevent_privilege priv) 
{
  kevent_t* event;
  kevent_hook_t* hook;

  if (!e_name || !function)
    return -1;

  /* No condition given means we always want to be called */
  if (!condition)
    condition = _always_hook_condition;

  event = kevent_get(e_name);

  if (!event)
    return -1; 

  hook = create_keventhook(function, condition, priv);

  if (!hook)
    return -1;

  /* Attach the hook, sorted by ID */
  if (kevent_attach_hook(event, hook)) {
    destroy_keventhook(hook);
    return -1;
  }

  return hook->m_id;
}

int kevent_do_unhook(char* e_name, uint32_t hook_id);

int kevent_hook_rehook(uint32_t hook_id, char* old_eventname, char* new_eventname);

int kevent_hook_set_privilege(uint32_t hook_id, enum kevent_privilege priv);
