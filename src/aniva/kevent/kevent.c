#include "kevent.h"
#include "kevent/context.h"
#include "kevent/eventhook.h"
#include "kevent/pipe.h"
#include "libk/flow/error.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include <crypto/k_crc32.h>

static mutex_t* __kevent_lock = nullptr;
static hashmap_t* __kevents_table = nullptr;
static zone_allocator_t* __kevent_allocator = nullptr;

static ErrorOrPtr __kevent_destroy_hooks(kevent_t* event)
{

  kevent_hook_t* next_hook;

  while (event->m_hooks) {

    next_hook = event->m_hooks->m_next;

    destroy_keventhook(event->m_hooks);

    event->m_hooks = next_hook;
  }

  return Success(0);
}

kevent_t* kevent_get(char* name) {
  kevent_t* ret;

  if (!name || !__kevents_table)
    return nullptr;

  ret = hashmap_get(__kevents_table, name);

  return ret;
}

/*
 * We attach keventhooks by sorting them by ID and putting them in
 * a linked list. When constructing the pipeline, we sort based on 
 * privilege
 */
ErrorOrPtr kevent_attach_hook(kevent_t* event, kevent_hook_t* hook) {
  
  kevent_hook_t** current_hook;

  if (!event || !hook)
    return Error();

  current_hook = &event->m_hooks;

  while (*current_hook && (*current_hook)->m_next) {

    kevent_hook_t* __current = *current_hook;

    if (__current->m_id < hook->m_id && __current->m_next->m_id > hook->m_id)
      break;
    
    current_hook = &__current->m_next;
  }

  if (*current_hook == nullptr) {
    *current_hook = hook;
    hook->m_next = nullptr;
  } else {
    hook->m_next = (*current_hook)->m_next;
    (*current_hook)->m_next = hook;
  }

  return Success(0);
}

ErrorOrPtr kevent_detach_hook(kevent_t* event, kevent_hook_t* hook){
  kernel_panic("TODO: implement kevent_detach_hook");
}

void init_kevents()
{
  __kevent_lock = create_mutex(0);

  __kevents_table = create_hashmap(KEVENT_MAX_EVENTS, HASHMAP_FLAG_SK);

  /* Create a zallocator for our kevents */
  __kevent_allocator = create_zone_allocator_ex(nullptr, NULL, sizeof(kevent_t) * (KEVENT_MAX_EVENTS), sizeof(kevent_t), NULL);

  ASSERT_MSG(__kevent_lock, "Failed to create kevent lock");
  ASSERT_MSG(__kevents_table, "Failed to create kevent named table");
  ASSERT_MSG(__kevent_allocator, "Failed to create kevent allocator");

  init_eventhooks();
}

ErrorOrPtr create_kevent(char* name, kevent_type_t type, uint32_t flags, size_t max_hook_count)
{
  const bool no_chaining = (flags & KEVENT_FLAG_NO_CHAINING) != KEVENT_FLAG_NO_CHAINING;
  //const bool nameless = (flags & KEVENT_FLAG_NAMELESS) != KEVENT_FLAG_NAMELESS;
  kevent_t* event;

  mutex_lock(__kevent_lock);

  if (!name)
    goto fail_and_unlock;

  if (hashmap_has(__kevents_table, name))
    goto fail_and_unlock;

  /* NOTE: If we don't specify a max amount of hooks AND we say we want to chain hooks, we fail */
  if (!max_hook_count && no_chaining)
    goto fail_and_unlock;

  event = zalloc_fixed(__kevent_allocator);

  memset(event, 0, sizeof(kevent_t));
  memcpy(event->m_name, name, strlen(name));

  if (no_chaining)
    max_hook_count = 1;

  if (!max_hook_count || max_hook_count > KEVENT_MAX_EVENT_HOOKS)
    max_hook_count = KEVENT_MAX_EVENT_HOOKS;

  event->m_hook_allocator = create_zone_allocator_ex(nullptr, NULL, sizeof(kevent_hook_t) * max_hook_count, sizeof(kevent_hook_t), NULL);
  event->m_type = type;
  event->m_flags = flags;

  event->m_hooks = nullptr;
  event->m_hooks_count = 0;
  event->m_max_hooks_count = max_hook_count;

  event->m_firing_lock = create_spinlock();

  /* TODO: id */
  event->m_id = 0;

  if (hashmap_put(__kevents_table, name, event).m_status != ANIVA_SUCCESS)
    goto fail_and_unlock;

  mutex_unlock(__kevent_lock);
  return Success(0);

fail_and_unlock:

  mutex_unlock(__kevent_lock);
  return Error();
}

ErrorOrPtr destroy_kevent(kevent_t* event)
{
  TRY(destroy_result, __kevent_destroy_hooks(event));

  destroy_spinlock(event->m_firing_lock);

  destroy_zone_allocator(event->m_hook_allocator, true);

  zfree_fixed(__kevent_allocator, event);

  return Success(0);
}

/*
 * NOTE: when doing this, we invalidate any lingering keys that might be 
 * laying around...
 */
ErrorOrPtr kevent_set_max_hooks(kevent_t** event, size_t new_max_hook_count)
{
  return Error();
}

ErrorOrPtr kevent_set_flags(kevent_t** event, uint32_t flags)
{
  return Error();
}

ErrorOrPtr fire_event(char* name, void* data)
{
  kevent_t* event;
  kevent_contex_t context;

  if (!name)
    return Error();

  /* 0) Find the event we need to fire */
  event = kevent_get(name);

  if (!event->m_hooks)
    return Error();

  /* TODO: let the caller detirmine if we are allowed to spin here */
  if (spinlock_is_locked(event->m_firing_lock))
    return Error();

  spinlock_lock(event->m_firing_lock);

  /* 1) Create initial context */
  context = (kevent_contex_t){
    .m_data = data,
    .m_flags = E_CONTEXT_FLAG_IMMUTABLE,
    .m_crc = 0,
  };

  context.m_crc = kcrc32(&context, sizeof(kevent_contex_t));

  /* 2) Construct pipeline */
  uintptr_t index;
  ErrorOrPtr result;
  kevent_contex_t* current_context;
  kevent_pipeline_t pipe = create_kevent_pipeline(event, &context);

  result = kpipeline_execute_next(&pipe);

  /* 3) Traverse the pipeline */
  while (result.m_status == ANIVA_SUCCESS && index++ <= KEVENT_MAX_EVENT_HOOKS) {
    result = kpipeline_execute_next(&pipe);

    if (result.m_status != ANIVA_SUCCESS)
      break;

    current_context = (kevent_contex_t*)Release(result);

    /* Canceled lol */
    if ((current_context->m_flags & E_CONTEXT_FLAG_CANCELED) == E_CONTEXT_FLAG_CANCELED)
      break;
  }

  /* Destroy the pipeline entry allocator */
  destroy_kevent_pipeline(&pipe);

  /* Remove the invalid context ptr */
  result.m_ptr = 0;

  /*
   * 4) Return the result of the traverse 
   */
unlock_and_exit:
  spinlock_unlock(event->m_firing_lock);

  return result;
}
