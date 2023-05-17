#ifndef __ANIVA_SYS_RESOURCE__
#define __ANIVA_SYS_RESOURCE__

#include "libk/error.h"
#include "sync/atomic_ptr.h"
#include "sync/mutex.h"
#include <libk/stddef.h>

/*
 * Every subcomponent of the kernel is able to request and obtain resources that can do different things.
 * a resource is categorised in a few different options:
 *  - Memory range
 *  - I/O range
 *  - IRQ mapping (line)
 * It is desired that every part of the kernel where resource-management is not trivial uses this mechanism
 * and obeys the rules that are attached to it. When a resource is taken, it is taken from a source (i.e. Memory,
 * the available IRQ lines, ect.). When the resource is released we should be able to mark the correct thing again 
 * so that it may be queried again, for example. A resource is always owned by a kernel object, so it is essential 
 * that we don't end up with dangeling pointers to resources, since that means weird stuff can happen. A resource 
 * is also always only owned by one kernel object, so when a resource is requested there is a mutex to be taken 
 * that ensures only one object has the resource at a time.
 */

enum kernel_resource_type;
enum kernel_resource_owner_type;
struct kernel_resource;
struct kernel_resource_ops;

typedef enum kernel_resource_type {
  RESOURCE_TYPE_IRQ = 0,
  RESOURCE_TYPE_MEM_RANGE,
  RESOURCE_TYPE_IO_RANGE,
} kernel_resource_type_t;

typedef enum kernel_resource_owner_type {
  RESOURCE_OWNER_TYPE_NONE = 0,
  RESOURCE_OWNER_TYPE_DRIVER,
  RESOURCE_OWNER_TYPE_KERNEL,
  RESOURCE_OWNER_TYPE_PROCESS,
  /* FIXME: Should we let resource be able to own other resources? */
  RESOURCE_OWNER_TYPE_RESOURCE,
} kernel_resource_owner_type_t;

typedef struct kernel_resource_ops {
  /* Read from the resource */
  uint8_t (*f_read8)(uintptr_t offset);
  uint16_t (*f_read16)(uintptr_t offset);
  uint32_t (*f_read32)(uintptr_t offset);
  uint64_t (*f_read64)(uintptr_t offset);

  /* Write to the resource */
  void (*f_write8)(uintptr_t offset, uint8_t data);
  void (*f_write16)(uintptr_t offset, uint16_t data);
  void (*f_write32)(uintptr_t offset, uint32_t data);
  void (*f_write64)(uintptr_t offset, uint64_t data);

  /* Detach the resource from its owner */
  ErrorOrPtr (*f_detach)(struct kernel_resource* resource);

} kernel_resource_ops_t;

typedef struct kernel_resource {
  
  /* Lock that protects the resource owning and useage */
  mutex_t* m_lock;

  kernel_resource_type_t m_type;
  kernel_resource_owner_type_t m_owner_type;

  char* m_name;

  /* Generic flat pointer to the data of the resource */
  uintptr_t m_data;
  size_t m_length;

  /* Object that owns the resource */
  void* m_owner;

  /* Some data that can differ per resource type */
  void* m_resource_specific_data;

  atomic_ptr_t* m_access_count;

  /* We link resources our own way, since we want to sort in a special way */
  struct kernel_resource* m_next;

  /* Generic resource operations */
  kernel_resource_ops_t* m_ops;

} kernel_resource_t;

/*
 * This is a pretty important function, so it should be called
 * quite early. This is because a lot of components of the kernel
 * use, or can use, resources
 */
void init_kernel_resources();

/*
 * Register a resource as in use by the kernel
 */
ErrorOrPtr register_resource(kernel_resource_t* resource);

/*
 * Free a resource to be used later
 */
ErrorOrPtr unregister_resource(kernel_resource_t* resource);

/*
 * Find a resource in the list of resources that the kernel knows about
 * by checking for its data
 */
kernel_resource_t* find_resource(kernel_resource_type_t type, uintptr_t data);
kernel_resource_t* find_resource_by_name(kernel_resource_type_t type, char* name);

kernel_resource_t* create_mem_range_resource(char* name, paddr_t physical_start, size_t length);
kernel_resource_t* create_irq_resource(char* name, uintptr_t irq_num);
kernel_resource_t* create_io_range_resource(char* name, uintptr_t start, uintptr_t end);

void destroy_resource(kernel_resource_t* resource);

ErrorOrPtr resource_attach_specific_data(kernel_resource_t* resource, void* data);
ErrorOrPtr resource_detach_specific_data(kernel_resource_t* resource);

ErrorOrPtr resource_attach_owner(kernel_resource_t* resource, void* owner);
ErrorOrPtr resource_detach_owner(kernel_resource_t* resource);

ErrorOrPtr resource_take(kernel_resource_t* resource, void* new_owner);


typedef struct {
  // Controller
  // Controller type

  // Active

  // Irq type
  // Attached handler
} irq_resource_data_t;

typedef struct {
  // Virtual address space 
  // Pagemap
  // Kernel-owned
} mem_range_resource_data_t;

typedef struct {
  // Access count
  // TODO: what else is specific to io ranges?
} io_range_resource_data_t;

#endif // !__ANIVA_SYS_RESOURCE__
