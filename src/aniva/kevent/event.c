#include "event.h"
#include "hook.h"
#include "kevent/hash.h"
#include "kevent/types/error/kerror.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"

static hashmap_t* _kevent_map = NULL;
static zone_allocator_t* _kevent_allocator = NULL;
static mutex_t* _kevent_lock = NULL;

/*
 * A few default events that are always active on the system
 *
 * These just come with the system bootstrap. Drivers may choose wether they use these default events,
 * or create their own events. They can then put their eventnames inside the profile variables of either
 * BASE or Global. For a keyboard driver, this might be something like this:
 * In the Global profile -> 'DEF_KB_EVNTNAME': 'myCoolEvent'
 */
struct {
  const char* name;
  enum KEVENT_TYPE type;
  uint32_t flags;
  uint32_t hook_capacity;
} default_kevents[] = {
  { "mouse",    KE_MOUSE_EVENT,     KEVENT_FLAG_FROZEN, 512 },
  { "error",    KE_ERROR_EVENT,     NULL, 128 },
  { "proc",     KE_PROC_EVENT,      NULL, 128 },
  { "thread",   KE_THREAD_EVENT,    NULL, 128 },
  { "profile",  KE_PROFILE_EVENT,   NULL, 128 },
  { "device",   KE_DEVICE_EVENT,    NULL, 128 },
  { "shutdown", KE_SHUTDOWN_EVENT,  NULL, 64  },
};
static const uint32_t default_kevent_count = sizeof(default_kevents) / sizeof(*default_kevents);

/*
 * Aniva kevent definition
 */
typedef struct kevent {
  const char* name;
  enum KEVENT_TYPE type;

  mutex_t* lock;

  uint32_t flags;
  uint32_t hook_count;
  uint32_t hook_capacity;
  uint32_t hooks_size;

  /* Hooks are registered both in this hash table and in a linked list */
  kevent_hook_t* hooks;

  /*
   * This is the head of the linked list and it links through all the hooks
   * We do this for an easy traverse when the event is fired
   */
  kevent_hook_t* hooks_head;
} kevent_t;


/*!
 * @brief: Allocate memory for a single kevent
 *
 */
static kevent_t* create_kevent(const char* name, enum KEVENT_TYPE type, uint32_t flags, uint32_t hook_capacity)
{
  kevent_t* event;

  if (!name || !_kevent_allocator)
    return nullptr;

  event = zalloc_fixed(_kevent_allocator);

  if (!event)
    return nullptr;

  memset(event, 0, sizeof(*event));

  event->type = type;
  event->flags = flags;
  event->name = strdup(name);
  event->lock = create_mutex(NULL);
  event->hooks_size = ALIGN_UP(hook_capacity * sizeof(kevent_hook_t), SMALL_PAGE_SIZE);
  event->hook_capacity = event->hooks_size / sizeof(kevent_hook_t);
  event->hooks = (kevent_hook_t*)Must(__kmem_kernel_alloc_range(event->hooks_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  memset(event->hooks, NULL, event->hooks_size);
  return event;
}

/*!
 * @brief: Clear memory used by the kevent
 */
static void destroy_kevent(kevent_t* event)
{
  kfree((void*)event->name);
  destroy_mutex(event->lock);

  __kmem_kernel_dealloc((vaddr_t)event->hooks, event->hook_capacity * sizeof(kevent_hook_t));
  zfree_fixed(_kevent_allocator, event);
}

/*!
 * @brief: Create a kevent and register it
 */
int add_kevent(const char* name, enum KEVENT_TYPE type, uint32_t flags, uint32_t hook_capacity)
{
  ErrorOrPtr err;
  kevent_t* event;

  if (!hook_capacity)
    hook_capacity = KEVENT_HOOK_SOFTMAX;

  event = kevent_get(name);

  /* Already present? */
  if (event)
    return -1;

  /* Create a new event */
  event = create_kevent(name, type, flags, hook_capacity);

  if (!event)
    return -2;

  mutex_lock(_kevent_lock);

  err = hashmap_put(_kevent_map, (hashmap_key_t)name, event);

  mutex_unlock(_kevent_lock);

  if (IsError(err))
    return -3;

  return 0;
}

/*!
 * @brief: unregister the kevent and destroy it
 */
int remove_kevent(const char* name)
{
  (void)destroy_kevent;
  kernel_panic("TODO: remove_kevent");
}

/*!
 * @brief: Prevent event fires
 */
int freeze_kevent(const char* name)
{
  kevent_t* event;

  event = kevent_get(name);

  return freeze_kevent_ex(event);
}

int freeze_kevent_ex(kevent_t* event)
{
  if (!event)
    return -1;

  if ((event->flags & KEVENT_FLAG_FROZEN) == KEVENT_FLAG_FROZEN)
    return 0;

  kevent_flag_set(event, KEVENT_FLAG_FROZEN);
  return 0;
}

/*!
 * @brief: Allow an event to fire again
 */
int unfreeze_kevent(const char* name)
{
  kevent_t* event;

  event = kevent_get(name);

  return unfreeze_kevent_ex(event);
}

int unfreeze_kevent_ex(struct kevent *event)
{
  if (!event)
    return -1;

  if ((event->flags & KEVENT_FLAG_FROZEN) != KEVENT_FLAG_FROZEN)
    return 0;

  kevent_flag_unset(event, KEVENT_FLAG_FROZEN);
  return 0;
}

/*!
 * @brief: Try to find an event based on name
 */
struct kevent* kevent_get(const char* name)
{
  kevent_t* ret;

  if (!name || !_kevent_lock)
    return nullptr;

  mutex_lock(_kevent_lock);

  ret = hashmap_get(_kevent_map, (hashmap_key_t)name);

  mutex_unlock(_kevent_lock);

  return ret;
}

void kevent_flag_set(struct kevent* event, uint32_t flags)
{
  mutex_lock(event->lock);

  event->flags |= flags;

  mutex_unlock(event->lock);
}

void kevent_flag_unset(struct kevent* event, uint32_t flags)
{
  mutex_lock(event->lock);

  event->flags &= ~(flags);

  mutex_unlock(event->lock);
}

bool kevent_flag_isset(struct kevent* event, uint32_t flags)
{
  return (event->flags & flags) == flags;
}

/*!
 * @brief: Tries to find a free entry for a hook
 */
static kevent_hook_t* kevent_get_free_hook_entry(kevent_t* event, const char* hook_name)
{
  uint32_t key;
  uint32_t i;
  uint32_t loops;
  kevent_hook_t* entry;

  if (!hook_name || !event || !event->hook_capacity)
    return nullptr;

  key = kevent_compute_hook_key(hook_name);
  i = (key % event->hook_capacity);
  loops = 0;

  /* Keep increasing the index until we hit an empty entry */
  do {
    /* Loop around if we reach the end */
    if (i >= event->hook_capacity)
      i = 0;

    entry = &event->hooks[i++];

    loops++;
  } while (keventhook_is_set(entry) && loops < event->hook_capacity);

  if (keventhook_is_set(entry))
    return nullptr;

  return entry;
}

/*!
 * @brief: Add a kevent hook to the link inside the kevent
 *
 * The events lock must held when calling this
 * This function is also responsible for handleing the events hook_count
 */
static void kevent_link_hook(kevent_t* event, kevent_hook_t* hook)
{
  kevent_hook_t* next = nullptr;

  if (event->hooks_head)
    next = event->hooks_head;

  hook->next = next;
  hook->prev = nullptr;

  if (event->hooks_head)
    event->hooks_head->prev = hook;

  event->hooks_head = hook;

  event->hook_count++;
}

/*!
 * @brief: Remove a kevent hook form the link inside the kevent
 *
 * The events lock must held when calling this
 * This function is also responsible for handleing the events hook_count
 *
 * We need to fix the following pointers:
 *  - The previous entry needs to point to our next entry
 *  - The next entry needs to point to our previous
 *  - The events head pointer might need fixing
 */
static void kevent_unlink_hook(kevent_t* event, kevent_hook_t* hook)
{
  kevent_hook_t* next = hook->next;
  kevent_hook_t* prev = hook->prev;

  if (!event->hook_count)
    return;

  if (next)
    next->prev = prev;

  if (prev)
    prev->next = next;

  if (event->hooks_head == hook)
    event->hooks_head = hook->next;

  hook->next = nullptr;
  hook->prev = nullptr;
  event->hook_count--;
}

/*!
 * @brief: Add a hook to the event
 *
 * 
 */
int kevent_add_hook(const char* event_name, const char* hook_name, int (*f_hook)(kevent_ctx_t*, void*), void* param)
{
  kevent_t* event;

  if (!event_name)
    return -1;

  event = kevent_get(event_name);

  if (!event)
    return -2;

  return kevent_add_hook_ex(event, hook_name, (FuncPtr)f_hook, KEVENT_HOOKTYPE_FUNC, NULL);
}

int kevent_add_poll_hook(const char* event_name, const char* hook_name, bool (*f_hook_should_fire)(kevent_ctx_t*, void*), void* param)
{
  kevent_t* event;

  if (!event_name)
    return -1;

  event = kevent_get(event_name);

  if (!event)
    return -2;

  return kevent_add_hook_ex(event, hook_name, (FuncPtr)f_hook_should_fire, KEVENT_HOOKTYPE_POLLABLE, param);
}

int kevent_add_hook_ex(struct kevent* event, const char* hook_name, FuncPtr f_hook, uint8_t hooktype, void* param)
{
  kevent_hook_t* hook;

  if (!event || !hook_name)
    return -1;

  /* Fuck */
  if (hooktype == KEVENT_HOOKTYPE_FUNC && !f_hook)
    return -KERR_INVAL;

  mutex_lock(event->lock);

  /* Do we already have this hook? */
  hook = kevent_get_hook(event, hook_name);

  /* Frick */
  if (hook)
    goto fail_and_exit;

  /* Is there a free entry for a new hook? */
  hook = kevent_get_free_hook_entry(event, hook_name);

  if (!hook)
    goto fail_and_exit;

  /* Yay, we found a spot */
  if (init_keventhook(hook, hook_name, hooktype, f_hook, param) < 0)
    goto fail_and_exit;

  kevent_link_hook(event, hook);

  mutex_unlock(event->lock);
  return 0;

fail_and_exit:
  mutex_unlock(event->lock);
  return -2;
}

/*!
 * @brief: Remove a hook from its event
 */
int kevent_remove_hook(const char* event_name, const char* hook_name)
{
  kevent_t* event;

  event = kevent_get(event_name);

  return kevent_remove_hook_ex(event, hook_name);
}

int kevent_remove_hook_ex(struct kevent *event, const char *hook_name)
{
  kevent_hook_t* hook;

  if (!event || !hook_name)
    return -1;

  mutex_lock(event->lock);

  /* Try to find the hook we want */
  hook = kevent_get_hook(event, hook_name);

  if (!hook)
    goto fail_and_exit;

  /* Unlink it */
  kevent_unlink_hook(event, hook);

  /* Destroy it */
  destroy_keventhook(hook);

  mutex_unlock(event->lock);
  return 0;

fail_and_exit:
  mutex_unlock(event->lock);
  return -1;
}

/*!
 * @brief: Try to find a hook based on it's name
 *
 * We hash the name and perform a linear search from there
 * this hopefully eliminates a lot of unneeded looping, but we do
 * have the advantage that there is a lot of stuff that fits into CPU cache
 */
struct kevent_hook* kevent_get_hook(struct kevent* event, const char* name)
{
  uint32_t key;
  uint32_t i;
  uint32_t loops;
  kevent_hook_t* entry;

  if (!name || !event || !event->hook_capacity)
    return nullptr;

  key = kevent_compute_hook_key(name);
  i = (key % event->hook_capacity);
  loops = 0;

  /* Keep increasing the index until we hit an entry that has the same string as our name */
  do {
    /* Loop around if we reach the end */
    if (i >= event->hook_capacity)
      i = 0;

    entry = &event->hooks[i++];

    loops++;
  } while (
      keventhook_is_set(entry) &&
      strcmp(name, entry->hookname) != 0 &&
      loops < event->hook_capacity
  );

  if (keventhook_is_set(entry) && strcmp(name, entry->hookname) == 0)
    return entry;

  return nullptr;
}

/*!
 * @brief: Also try to find a hook, but linear search this time
 *
 * This is here if u don't trust my hashing stuff =)
 */
struct kevent_hook* kevent_scan_hook(struct kevent* event, const char* name)
{
  kevent_hook_t* ret;

  for (uint32_t i = 0; i < event->hook_capacity; i++) {
    ret = &event->hooks[i];

    if (!keventhook_is_set(ret))
      continue;

    if (strcmp(name, ret->hookname) == 0)
      return ret;
  }

  return nullptr;
}

/*!
 * @brief: Find the kevent with @name and fire it
 */
int kevent_fire(const char* name, void* buffer, size_t size)
{
  kevent_t* event;

  event = kevent_get(name);

  if (!event)
    return -1;

  /* No need to even do jackshit */
  if (!event->hook_count)
    return 0;

  return kevent_fire_ex(event, buffer, size);
}

/*!
 * @brief: Fire a kevent
 *
 * Walk it's linked hooks and fire the events one by one 
 */
int kevent_fire_ex(struct kevent* event, void* buffer, size_t size)
{
  int error;
  processor_t* c_cpu;
  thread_t* c_thread;
  kevent_ctx_t ctx;
  kevent_hook_t* current_hook;

  mutex_lock(event->lock);

  /* This event has been frozen. Skip this fire =/ */
  if (kevent_flag_isset(event, KEVENT_FLAG_FROZEN))
    return 1;

  current_hook = event->hooks_head;

  if (!current_hook)
    return 2;

  /* Create a context (TODO: move to own function) */
  ctx = (kevent_ctx_t) {
    .event = event,
    .buffer = buffer,
    .buffer_size = size,
    .flags = { 0 },
    0,
  };

  c_cpu = get_current_processor();
  c_thread = get_current_scheduling_thread();

  if (c_cpu->m_irq_depth)
    ctx.flags.in_irq = true;

  /* TODO: check if current proc is a userproc
   */

  /*
   * Create the original fullid
   * Why don't we simply pass the pointers to the event context?
   * well the original process or thread might end while firing this event, making
   * the pointers invalid. By passing the pid and tid to the context, any eventhook
   * can know if the process or thread is still active, before trying to access them
   *
   * This does raise a few other weird behavioural possibilities that we need to account
   * for, but these are easier to deal with than with lingering pointers
   *
   * (NOTE: perhaps we create a better system to track pointer validities
   * in which case we probably need to switch kevent contexts back to pointers =D ) 
   */
  ctx.orig_cpu = c_cpu;
  ctx.orig_fid = create_full_procid(c_thread->parent_proc->m_id, c_thread->tid);

  do {

    error = keventhook_call(current_hook, &ctx);

    if (error && kevent_flag_isset(event, KEVENT_FLAG_ERROR_FATAL))
      break;

    current_hook = current_hook->next;
  } while (current_hook);

  mutex_unlock(event->lock);
  return error;
}

/*!
 * @brief: Wait for a pollable keventhook to fire
 *
 * 
 */
int kevent_await_hook_fire(const char* event_name, const char* hook_name, uint64_t timeout, struct kevent_hook_poll_block** pblock)
{
  int error;
  kevent_t* event;
  kevent_hook_t* hook;
  kevent_hook_poll_block_t* block;

  if (!event_name || !hook_name)
    return -KERR_INVAL;

  /* Grab the event we want to wait for */
  event = kevent_get(event_name);

  if (!event)
    return -KERR_NOT_FOUND;

  /* Grab the hook we're interested in */
  hook = kevent_get_hook(event, hook_name);

  if (!hook)
    return -KERR_NOT_FOUND;

  do {
    /* Fuck bro */
    scheduler_yield();

    /* Zero this bitch real quick */
    block = NULL;

    /* Try to dequeue a poll block */
    error = keventhook_dequeue_poll_block(hook, &block);

    if (timeout)
      if (timeout-- == 0)
        break;

  } while (error);

  if (error)
    return error;

  if (!block)
    return -KERR_NULL;

  /* Export the block to the caller if it wants it. Otherwise just kill it lmao */
  if (pblock) *pblock = block;
  else destroy_keventhook_poll_block(block);

  return 0;
}

enum KEVENT_TYPE kevent_get_type(struct kevent* event)
{
  return event->type;
}

/*!
 * @brief: Allocate structures needed for kevent to function
 */
void init_kevents()
{
  _kevent_allocator = create_zone_allocator(128 * Kib, sizeof(kevent_t), NULL);
  _kevent_lock = create_mutex(NULL);
  _kevent_map = create_hashmap(KEVENT_SOFTMAX_COUNT, HASHMAP_FLAG_SK);

  /* Create the default kernel events */
  for (uint32_t i = 0; i < default_kevent_count; i++) {
    ASSERT_MSG(add_kevent(
      default_kevents[i].name,
      default_kevents[i].type,
      default_kevents[i].flags | KEVENT_FLAG_DEFAULT,
      default_kevents[i].hook_capacity
    ) == 0,
    "Failed to create default kevents");
  }

  kevent_add_hook("error", "default", __default_kerror_handler, NULL);
}
