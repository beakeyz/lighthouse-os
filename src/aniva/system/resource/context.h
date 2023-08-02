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
} resource_ctx_t;

/*
 * This struct represents the stack that we use to keep track of every context
 *  entering a context means pushing onto the stack
 *  exiting a context means popping of of the stack
 * Every stack has a HARD max depth, meaning that when this gets overrun, we consider this a 
 * fatal error and we panic =)
 * A resource context should also be able to cover a bit more than regular kresources,
 * since these are also meant to be used to keep track of resources allocated by drivers and kernel
 * subsystems. Processes will have enough with just kresources for now...
 */
typedef struct resource_ctx_stack {
  uintptr_t stack_index;
  uintptr_t stack_capacity;

  resource_ctx_t entries[];
} resource_ctx_stack_t;


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
