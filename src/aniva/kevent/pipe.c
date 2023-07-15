#include "pipe.h"
#include "crypto/k_crc32.h"
#include "kevent/kevent.h"
#include "libk/flow/error.h"
#include "mem/zalloc.h"

kevent_pipeline_t create_kevent_pipeline(kevent_t* event, kevent_contex_t* context) {
  kevent_pipeline_t pipe = { 0 };

  if (!event || !context)
    return pipe;

  pipe = (kevent_pipeline_t) {
    .m_head = nullptr,
    .m_context = context,
    .m_entry_allocator = create_zone_allocator_ex(nullptr, NULL, sizeof(kevent_pipe_entry_t) * KEVENT_MAX_EVENT_HOOKS, sizeof(kevent_pipe_entry_t), NULL),
  };

  kevent_pipe_entry_t** current_pentry = &pipe.m_head;

  /* Loop over all the hooks to sort them based on privilege */
  for (uint32_t i = 0; i < KEVENT_PRIVILEGE_COUNT; i++) {
    
    kevent_hook_t** current;
    kevent_privilege_t priv = i;

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
  
  return pipe;
}

void destroy_kevent_pipeline(kevent_pipeline_t* pipeline) {
  if (!pipeline)
    return;

  /* Destroying the allocater also drops all the allocations made */
  destroy_zone_allocator(pipeline->m_entry_allocator, true);

  memset(pipeline, 0, sizeof(kevent_pipeline_t));
}

ErrorOrPtr kpipeline_execute_next(kevent_pipeline_t* pipeline) {

  uint32_t crc_check;
  uint32_t crc_copy;
  kevent_contex_t* context;
  kevent_contex_t context_copy;

  if (!pipeline || !pipeline->m_head)
    return Error();

  context = pipeline->m_context;

  /* Cache the crc */
  crc_copy = context->m_crc;

  /* Zero the crc and flags */
  context->m_crc = 0;
  context->m_flags = 0;

  /* Execute the hook */
  context = pipeline->m_head->f_pipeline_func(context);

  /* Cache the context, post-execute */
  context_copy = *context;

  context_copy.m_crc = 0;
  context_copy.m_flags = 0;

  crc_check = kcrc32(&context_copy, sizeof(context_copy));

  /* Check if the crc changed inside the execution of the hook */
  if (crc_copy != crc_check) {
    context->m_flags |= E_CONTEXT_FLAG_INVALID;
    return Error();
  }

  /* Shift the head forward */
  pipeline->m_head = pipeline->m_head->m_next;

  /* Return the new context */
  return Success((uintptr_t)context); 
}
