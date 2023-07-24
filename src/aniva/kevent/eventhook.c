#include "eventhook.h"
#include "crypto/k_crc32.h"
#include "kevent/kevent.h"
#include "libk/flow/error.h"
#include "mem/zalloc.h"

static zone_allocator_t* __kevent_hook_allocator = nullptr;

void init_eventhooks() {

  /* ALlocator for the hooks */
  __kevent_hook_allocator = create_zone_allocator_ex(nullptr, NULL, sizeof(kevent_hook_t) * KEVENT_MAX_EVENT_HOOKS, sizeof(kevent_hook_t), NULL); 
}

static ErrorOrPtr __validate_keventhook(kevent_hook_t* hook)
{
  uint32_t check_id;
  kevent_hook_t copy;

  if (!hook)
    return Error();

  copy = *hook;

  copy.m_next = nullptr;
  copy.m_id = 0;

  check_id = kcrc32(&copy, sizeof(kevent_hook_t));

  /* ID mismatch */
  if (check_id != hook->m_id)
    return Error();

  return Success(0);
}

kevent_hook_t* create_keventhook(kevent_hook_fn_t function, kevent_hook_condition_fn_t condition, kevent_privilege_t priv) {

  if (!__kevent_hook_allocator)
    return nullptr;

  kevent_hook_t* ret = zalloc_fixed(__kevent_hook_allocator);

  memset(ret, 0, sizeof(kevent_hook_t));

  ret->m_function = function;
  ret->m_hook_condition = condition;
  ret->m_privilege = priv;

  ret->m_id = kcrc32(ret, sizeof(kevent_hook_t));

  return ret;
}

void destroy_keventhook(kevent_hook_t* hook) {

  zfree_fixed(__kevent_hook_allocator, hook);
}

ErrorOrPtr kevent_do_hook(char* e_name, kevent_hook_fn_t function, kevent_hook_condition_fn_t condition, kevent_privilege_t priv) {
  
  kevent_t* event;
  kevent_hook_t* hook;

  if (!e_name || !function || !condition)
    return Error();

  event = kevent_get(e_name);

  if (!event)
    return Error(); 

  hook = create_keventhook(function, condition, priv);

  if (!hook)
    return Error();

  /* Attach the hook, sorted by ID */
  TRY(attach_result, kevent_attach_hook(event, hook));

  return Success(hook->m_id);
}

ErrorOrPtr kevent_do_unhook(char* e_name, uint32_t hook_id);

ErrorOrPtr kevent_hook_rehook(uint32_t hook_id, char* old_eventname, char* new_eventname);

ErrorOrPtr kevent_hook_set_privilege(uint32_t hook_id, kevent_privilege_t priv);
