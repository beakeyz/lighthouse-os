#ifndef __ANIVA_RESOURCE_CONTEXT__
#define __ANIVA_RESOURCE_CONTEXT__
#include <libk/stddef.h>
#include <system/resource.h>

/*
 * Resource context are meant to allow us to track which 
 * kind of entity (and who specifically) is trying to allocate resources,
 * without them having to specify each time that they are doing the allocation 
 * (since that way they can easily pretend to be anything else)
 * Thats why we allow the kernel to regulate who is allocating, so that 
 * the entities can simply allocate to their hearts content, and we as the kernel
 * will know how to clean up their mess every time =D
 */

struct Processor;
struct proc;
struct dev_manifest;

#define RCTX_FLAG_KERNEL (0x00000001)
#define RCTX_FLAG_DRIVER (0x00000002)
#define RCTX_FLAG_USER   (0x00000004)

typedef struct resource_context {
  uint32_t flags;
  uint32_t res0;
  kresource_bundle_t resource_bundle;

  struct Processor* parent_cpu;

  /* The entity this is linked to */
  union {
    struct dev_manifest* driver;
    struct proc* process;
  };

  /* Next context in the chain */
  struct resource_context* next;
} resource_ctx_t;

/*
 * Initialization
 */
void init_resource_ctx();

void destroy_resource_ctx(resource_ctx_t* ctx);
resource_ctx_t* create_resource_ctx();

resource_ctx_t* get_current_resource_ctx();

/* Add a context to the chain, so we automatically enter this one after we exit the previous */
void chain_resource_ctx(resource_ctx_t* ctx);

/* Remove a context from the chain */
void remove_resource_ctx(resource_ctx_t* ctx);

/* Imidialtely enter a context */
void enter_resource_ctx(resource_ctx_t* ctx);

/* Exit a resource context */
void exit_resource_ctx(resource_ctx_t* ctx);

bool resource_ctx_is_last(resource_ctx_t* ctx);

#endif // !__ANIVA_RESOURCE_CONTEXT__
