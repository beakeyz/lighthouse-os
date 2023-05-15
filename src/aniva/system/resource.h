#ifndef __ANIVA_SYS_RESOURCE__
#define __ANIVA_SYS_RESOURCE__

#include "libk/error.h"
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
struct kernel_resource;
struct kernel_resource_ops;

typedef enum kernel_resource_type {
  RESOURCE_TYPE_IRQ = 0,
  RESOURCE_TYPE_MEM_RANGE = 0,
  RESOURCE_TYPE_IO_RANGE = 0,
} kernel_resource_type_t;

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
  
  /* Lock that protects the resource owning */
  mutex_t* m_lock;

  kernel_resource_type_t m_type;

  /* Generic flat pointer to the data of the resource */
  uintptr_t m_data;
  /* size of the resource */
  size_t m_length;

  /* Object that owns the resource */
  void* m_owner;

  /* Generic resource operations */
  kernel_resource_ops_t* m_ops;

} kernel_resource_t;

#endif // !__ANIVA_SYS_RESOURCE__
