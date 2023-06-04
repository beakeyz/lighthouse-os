#include "kevent.h"
#include "libk/error.h"
#include "libk/hashmap.h"
#include "libk/linkedlist.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include "sync/spinlock.h"
#include <crypto/k_crc32.h>

#define KEVENT_MAX_NAMED_EVENTS 256
#define KEVENT_MAX_UNNAMED_EVENTS 1024
#define KEVENT_MAX_EVENTS (KEVENT_MAX_NAMED_EVENTS + KEVENT_MAX_UNNAMED_EVENTS)

static mutex_t* __kevent_lock = nullptr;

static hashmap_t* __kevent_named_table = nullptr;
static list_t* __kevent_unnamed_table[KEVENT_TYPE_COUNT] = { 0 };

static zone_allocator_t* __kevent_allocator = nullptr;

static ErrorOrPtr __validate_kevent(kevent_t* event, kevent_key_t key) 
{
  kevent_t copy = *event;

  /* Mask mutable fields */
  copy.m_hooks_count = 0;
  copy.m_hooks = 0;
  copy.m_key = 0;

  uint32_t check_crc = kcrc32(&copy, sizeof(kevent_t));
  
  if (check_crc == event->m_key)
    return Success(0);

  return Error();
}

void init_kevents()
{
  __kevent_lock = create_mutex(0);

  __kevent_named_table = create_hashmap(KEVENT_MAX_NAMED_EVENTS, HASHMAP_FLAG_STR_KEY);

  for (uint32_t i = 0; i < KEVENT_TYPE_COUNT; i++) {
    __kevent_unnamed_table[i] = init_list();
  }

  /* Create a zallocator for our kevents */
  __kevent_allocator = create_zone_allocator_ex(nullptr, NULL, sizeof(kevent_t) * (KEVENT_MAX_EVENTS), sizeof(kevent_t), ZALLOC_FLAG_FIXED_SIZE);

  ASSERT_MSG(__kevent_lock, "Failed to create kevent lock");
  ASSERT_MSG(__kevent_named_table, "Failed to create kevent named table");
  ASSERT_MSG(__kevent_unnamed_table, "Failed to create kevent unnamed table");
  ASSERT_MSG(__kevent_allocator, "Failed to create kevent allocator");
  ASSERT_MSG(__kevent_allocator->m_max_zone_size == sizeof(kevent_t), "kevent allocator size mismatch");
}

ErrorOrPtr create_kevent(char* name, kevent_type_t type, uint32_t flags, size_t max_hook_count)
{

  const bool no_chaining = (flags & KEVENT_FLAG_NO_CHAINING) != KEVENT_FLAG_NO_CHAINING;
  const bool nameless = (flags & KEVENT_FLAG_NAMELESS) != KEVENT_FLAG_NAMELESS;
  kevent_t* event;

  mutex_lock(__kevent_lock);

  /* NOTE: If we don't specify a max amount of hooks AND we say we want to chain hooks, we fail */
  if (!max_hook_count && no_chaining)
    goto fail_and_unlock;

  if (!name && nameless)
    goto fail_and_unlock;

  event = zalloc_fixed(__kevent_allocator);

  memset(event, 0, sizeof(kevent_t));

  if (name) 
    memcpy(event->m_name, name, strlen(name));

  if (no_chaining)
    max_hook_count = 1;

  event->m_hook_allocator = create_zone_allocator_ex(nullptr, NULL, sizeof(kevent_hook_t) * max_hook_count, sizeof(kevent_hook_t), ZALLOC_FLAG_FIXED_SIZE);
  event->m_type = type;
  event->m_flags = flags;

  event->m_hooks = nullptr;
  event->m_hooks_count = 0;
  event->m_max_hooks_count = max_hook_count;

  event->m_firing_lock = create_spinlock();

  event->m_key = kcrc32(event, sizeof(kevent_t));

  /* TODO: id */
  event->m_id = 0;

  if (nameless) {
    list_append(__kevent_unnamed_table[type], event);
  } else {
    if (hashmap_put(__kevent_named_table, name, event).m_status != ANIVA_SUCCESS)
      goto fail_and_unlock;
  }

  mutex_unlock(__kevent_lock);
  return Success(event->m_key);

fail_and_unlock:

  mutex_unlock(__kevent_lock);
  return Error();
}

ErrorOrPtr destroy_kevent(char* name, kevent_key_t key)
{
  return Error();
}

ErrorOrPtr kevent_set_max_hooks(kevent_t** event, size_t new_max_hook_count)
{
  return Error();
}

ErrorOrPtr kevent_set_flags(kevent_t** event, uint32_t flags)
{
  return Error();
}

ErrorOrPtr fire_event(char* name, kevent_key_t key, void* data)
{
  /* 0) Find the event we need to fire */
  /* 1) Create initial context */
  /* 2) Construct pipeline */
  /* 3) Traverse the pipeline */
  /* 4) Return the result of the traverse */
  return Error();
}

ErrorOrPtr fire_event_id(uint32_t id, kevent_type_t type, kevent_key_t key, void* data)
{
  return Error();
}
