#include "sys_proc.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

/*!
 * @brief: Get the processes runtime context
 */
const char* sys_get_runtime_ctx()
{
  proc_t* c_proc;
  size_t ctx_len;
  const char* ctx;
  vaddr_t ctx_buffer;

  c_proc = get_current_proc();

  if (!KERR_OK(proc_get_runtime(c_proc, &ctx)) || !ctx)
    return NULL;

  ctx_len = strlen(ctx);

  if (!ctx_len)
    return NULL;

  ctx_buffer = Must(kmem_user_alloc_range(c_proc, ctx_len, NULL, NULL));

  memcpy((void*)ctx_buffer, ctx, ctx_len);

  return (const char*)ctx_buffer;
}
