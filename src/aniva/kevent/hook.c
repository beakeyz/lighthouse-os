#include "hook.h"
#include "kevent/hash.h"
#include "mem/heap.h"
#include <libk/stddef.h>
#include <libk/string.h>

/*!
 * @brief: Initialize memory used by the hook
 */
int init_keventhook(kevent_hook_t* hook, const char* name, f_hook_fn hook_fn)
{
  if (!hook)
    return -1;

  /* Should be destroyed first */
  if (keventhook_is_set(hook))
    return -2;

  memset(hook, 0, sizeof(*hook));

  hook->key = kevent_compute_hook_key(name);
  hook->hookname = strdup(name);
  hook->f_hook = hook_fn;

  /* Make sure we know this hook is set */
  hook->is_set = true;

  return 0;
}

/*!
 * @brief: Clear memory used by the hook
 */
void destroy_keventhook(kevent_hook_t* hook)
{
  kfree((void*)hook->hookname);

  memset(hook, 0, sizeof(*hook));
}

/*!
 * @brief: Try to call the hooks function
 *
 * Returns -1 if the hook has no function
 */
int keventhook_call(kevent_hook_t* hook, kevent_ctx_t* ctx)
{
  if (!hook->f_hook)
    return -1;

  return hook->f_hook(ctx);
}

/*!
 * @brief: Check if a hook is used
 */
bool keventhook_is_set(kevent_hook_t* hook)
{
  return (hook->is_set || hook->hookname || hook->f_hook);
}

