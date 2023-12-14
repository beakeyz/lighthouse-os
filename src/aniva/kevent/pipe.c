#include "pipe.h"
#include "crypto/k_crc32.h"
#include "kevent/context.h"
#include "kevent/kevent.h"
#include "libk/flow/error.h"
#include "mem/zalloc.h"

uint32_t _dummy_pipe_func(kevent_contex_t context)
{
  return 0;
}

static kevent_pipe_entry_t _dummy_pipe_entry = {
  .m_next = nullptr,
  .f_pipeline_func = _dummy_pipe_func,
};

int create_kevent_pipeline(kevent_pipeline_t* out, struct kevent* event, kevent_contex_t* context)
{
  kevent_pipeline_t pipe;

  if (!out || !event || !context)
    return -1;

  /*
   * NOTE: m_exec_head is null until we start an execute
   */
  pipe = (kevent_pipeline_t) {
    .m_exec_head = nullptr,
    .m_entries = nullptr,
    .m_context = context,
    .m_entry_allocator = create_zone_allocator_ex(nullptr, NULL, sizeof(kevent_pipe_entry_t) * KEVENT_MAX_EVENT_HOOKS, sizeof(kevent_pipe_entry_t), NULL),
  };

  /* Fill the entries */
  kevent_pipe_entry_t** current_pentry = &pipe.m_entries;

  /* Loop over all the hooks to sort them based on privilege */
  for (uint32_t i = 0; i < KEVENT_PRIVILEGE_COUNT; i++) {
    
    kevent_hook_t** current;
    enum kevent_privilege priv = i;

    for (current = &event->m_hooks; *current; current = &(*current)->m_next) {

      /* Filter out the hooks that don't meet the condition */
      if (!(*current)->m_hook_condition(*context)) {
        continue;
      }
      
      /* Privilege matches */
      if ((*current)->m_privilege == priv) {
        /* Create a pipe entry */
        kevent_pipe_entry_t* entry = zalloc_fixed(pipe.m_entry_allocator);
        entry->f_pipeline_func = (*current)->m_function;
        entry->m_next = nullptr;

        /* Fill this entry */
        *current_pentry = entry;

        /* Update the pentry pointer */
        current_pentry = &entry->m_next;
      }
    }
  }

  *out = pipe;
  return 0;
}

void destroy_kevent_pipeline(kevent_pipeline_t* pipeline) 
{
  if (!pipeline)
    return;

  /* Destroying the allocater also drops all the allocations made */
  destroy_zone_allocator(pipeline->m_entry_allocator, true);

  memset(pipeline, 0, sizeof(kevent_pipeline_t));
}

/*!
 * @brief: Get a safe exec head
 */
static inline kevent_pipe_entry_t* ke_pipeline_get_exec_head(kevent_pipeline_t* pipeline)
{
  if (!pipeline->m_exec_head)
    return &_dummy_pipe_entry;

  return pipeline->m_exec_head;
}

int kpipeline_execute_next(kevent_pipeline_t* pipeline)
{
  uint32_t flags;
  uint32_t crc_check;
  uint32_t crc_copy;
  kevent_contex_t context;
  kevent_contex_t context_copy;

  if (!pipeline || !pipeline->m_entries)
    return -1;

  if (!pipeline->m_exec_head)
    pipeline->m_exec_head = pipeline->m_entries;

  context = *pipeline->m_context;

  /* Execute the hook */
  flags = ke_pipeline_get_exec_head(pipeline)->f_pipeline_func(context);

  /* Caller has direct control over the flags of the context */
  pipeline->m_context->m_flags = flags;

  /* Shift the head forward */
  pipeline->m_exec_head = pipeline->m_exec_head->m_next;

  /* Return the new context */
  return 0; 
}
