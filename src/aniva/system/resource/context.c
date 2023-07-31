#include "context.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include "system/asm_specifics.h"
#include "system/processor/processor.h"

static resource_ctx_t* __resource_contexts;
static mutex_t* __resource_context_lock;
static zone_allocator_t* __res_ctx_allocator;

static resource_ctx_t* alloc_resource_ctx()
{
  return zalloc_fixed(__res_ctx_allocator);
}

static void free_resource_ctx(resource_ctx_t* ctx)
{
  zfree_fixed(__res_ctx_allocator, ctx);
}

void destroy_resource_ctx(resource_ctx_t* ctx)
{
  /* TODO: clean up the resource bundle */
  free_resource_ctx(ctx);
}

resource_ctx_t* create_resource_ctx()
{
  resource_ctx_t* ret = alloc_resource_ctx();

  (void)ret;

  return ret;
}

resource_ctx_t* get_current_resource_ctx()
{
  /*
   * Since the current resource context is stored in the local CPU, use the GS base (where we store the current CPU struct)
   * to locate our current context
   */
  return (resource_ctx_t*)read_gs(GET_OFFSET(Processor_t, m_current_resource_ctx));
}

bool resource_ctx_is_last(resource_ctx_t* ctx)
{
  /*
   * The last context should always be that of the kernel 
   * any other contexts (like that of drivers or processes) should
   * be linked behind this one at all times. This way we can simply
   * move to the next context in the link and remove the previous, when
   * we exit a context
   */
  return (ctx->next == nullptr);
}

void init_resource_ctx()
{
  /* Create the root context (that of the kernel) */
  __resource_contexts = create_resource_ctx();

  /* Create the mutext */
  __resource_context_lock = create_mutex(NULL);

  mutex_lock(__resource_context_lock);

  /* Create the allocator */
  __res_ctx_allocator = create_zone_allocator(SMALL_PAGE_SIZE, sizeof(resource_ctx_t), NULL);

  /* Make sure that this is empty */
  memset(__resource_contexts->resource_bundle, 0, sizeof(kresource_bundle_t));

  mutex_unlock(__resource_context_lock);
}
