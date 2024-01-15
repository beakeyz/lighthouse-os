#ifndef __ANIVA_SYS_RESOURCE__
#define __ANIVA_SYS_RESOURCE__

#include "dev/driver.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "mem/page_dir.h"
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

struct kresource;

/*
 * NOTE: types should be going from 0
 * to KRES_MAX_TYPE with no gaps
 */
#define KRES_TYPE_IO            (0)
#define KRES_TYPE_MEM           (1)
#define KRES_TYPE_IRQ           (2)

#define KRES_MIN_TYPE           (KRES_TYPE_IO)
#define KRES_MAX_TYPE           (KRES_TYPE_IRQ)

#define KRES_TYPE_COUNT         (KRES_MAX_TYPE+1)

#define KRES_FLAG_MEM_KEEP_PHYS (0x0001) /* Make sure we don't touch physical stuff when releasing this badboi (This flag is only used for memory resources)*/
#define KRES_FLAG_IO__RES0      (0x0001) /* TODO: find usage */
#define KRES_FLAG_IRQ_RES0      (0x0001) /* TODO: find usage */

#define KRES_FLAG_SHARED        (0x0002) /* This resource has a refcount of > 1 */
#define KRES_FLAG_NEVER_SHARE   (0x0004) /* Never share this resource */
#define KRES_FLAG_PROTECTED_R   (0x0008) /* Protected resource: reads should be checked */
#define KRES_FLAG_PROTECTED_W   (0x0010) /* Protected resource: writes should be checked */
#define KRES_FLAG_PROTECTED_RW  (KRES_FLAG_PROTECTED_R | KRES_FLAG_PROTECTED_W) /* Both reads and writes are protected */
#define KRES_FLAG_UNBACKED      (0x0020) /* This resource is merely commited. We need to make sure it is backed on-demand. When the refcount is zero, this flag is assumed */
#define KRES_FLAG_ONDISK        (0x0040) /* We can find the actual data on disk =) (TODO: figure out how to get to it) */

typedef uint16_t kresource_type_t; 
typedef uint16_t kresource_flags_t;

void init_kresources();

/*
 * This struct should be optimally packed, since we will use this
 * to store the actual resource usage
 * NOTE: memory resources work on virtual memory
 */
typedef struct kresource {
  
  const char* m_name;

  /* Function that prompts the resources deletion and releases the physical resource it holds (memory, irqs, io ports) */
  // TODO: implement fully
  int (*f_release) (struct kresource*);

  /* Data regarding the resource */
  uintptr_t m_start;
  size_t m_size;

  /* Info about the type of resource */
  kresource_type_t m_type;
  kresource_flags_t m_flags;

  /* How many shares does this resource have? */
  flat_refc_t m_shared_count;

  /* 
   * The original owner of the resource 
   * when the owner is null but the resource is referenced,
   * we are dealing with an anonymous resource
   */
  void* m_owner;

  /* Always have resources grouped together */
  struct kresource* m_next;
} kresource_t;

static inline bool resource_contains(kresource_t* resource, uintptr_t address) 
{
  if (!resource)
    return false;

  return (address >= resource->m_start && address < (resource->m_start + resource->m_size));
}


static inline bool resource_is_anonymous(kresource_t* resource)
{
  return (!resource->m_owner && resource->m_shared_count);
}

/*
 * Bundle containing every resource type and information that is shared between
 * all resource entries
 */
typedef struct kresource_bundle { 
  page_dir_t* page_dir;
  kresource_t* resources[3];
} kresource_bundle_t;

kresource_bundle_t* create_resource_bundle(page_dir_t* dir);

/*
 * Destroy every kresource in the bundle
 */
void destroy_resource_bundle(kresource_bundle_t* bundle);

#define GET_RESOURCE(bundle, type) (bundle->resources[type])

/*
 * Flat out free the memory for this resource. No fixing the 
 * pointers of the list its in
 */
void destroy_kresource(kresource_t* resource);

/*
 * If the requested resource is already used, we just increment the refcount
 * otherwise we can splice off a new resource and have it fresh
 * when regions is null we will use the default system list
 * TODO: integrate flags
 */
ErrorOrPtr resource_claim_ex(const char* name, void* owner, uintptr_t start, size_t size, kresource_type_t type, kresource_bundle_t* bundle);
ErrorOrPtr resource_claim_kernel(const char* name, void* owner, uintptr_t start, size_t size, kresource_type_t type);

ErrorOrPtr resource_commit(uintptr_t start, size_t size, kresource_type_t type, kresource_bundle_t* bundle);

ErrorOrPtr resource_apply_flags(uintptr_t start, size_t size, kresource_flags_t flags, kresource_t* resources_start);

/*
 * Resource release routines
 * region is the adress of the pointer to the region right before the one we should kill, so
 * that we know whether we can merge them
 * Returns:
 * - Success -> the resource is completely free and unreferenced entirely throughout
 * - Warning -> the resource was successfully unreferenced, but parts of it are still referenced
 * - Error   -> something went wrong while trying to release
 */
ErrorOrPtr resource_release_region(kresource_t* previous_region, kresource_t* current_region);
ErrorOrPtr resource_release(uintptr_t start, size_t size, kresource_t* mirrors_start);

ErrorOrPtr resource_clear_owned(void* owner, kresource_type_t type, kresource_bundle_t* bundle);

ErrorOrPtr resource_find_usable_range(kresource_bundle_t* bundle, kresource_type_t type, size_t size);
#endif // !__ANIVA_SYS_RESOURCE__
