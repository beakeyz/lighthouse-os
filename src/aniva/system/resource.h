#ifndef __ANIVA_SYS_RESOURCE__
#define __ANIVA_SYS_RESOURCE__

#include "dev/driver.h"
#include "libk/error.h"
#include "libk/reference.h"
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
struct kresource_mirror;

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

#define KRES_FLAG_BASE_RESOURCE (0x00000001) /* This resource is the parent of all other resources */
#define KRES_FLAG_SHARED        (0x00000002) /* This resource has a refcount of > 1 */
#define KRES_FLAG_NEVER_SHARE   (0x00000004) /* Never share this resource */
#define KRES_FLAG_PROTECTED_R   (0x00000008) /* Protected resource: reads should be checked */
#define KRES_FLAG_PROTECTED_W   (0x00000010) /* Protected resource: writes should be checked */
#define KRES_FLAG_PROTECTED_RW  (KRES_FLAG_PROTECTED_R | KRES_FLAG_PROTECTED_W) /* Both reads and writes are protected */

typedef uint16_t kresource_type_t; 
typedef uint16_t kresource_flags_t;

void init_kresources();

/*
 * This struct should be optimally packed, since we will use this
 * to store the actual resource usage
 * NOTE: memory resources work on virtual memory
 */
typedef struct kresource {

  /* Data regarding the resource */
  uintptr_t m_start;
  size_t m_size;

  /* Info about the type of resource */
  kresource_type_t m_type;
  kresource_flags_t m_flags;

  /* How many shares does this resource have? */
  flat_refc_t m_shared_count;

  /* Always have resources grouped together */
  struct kresource* m_next;
} kresource_t;

void debug_resources(kresource_type_t type);

/*
 * If the requested resource is already used, we just increment the refcount
 * otherwise we can splice off a new resource and have it fresh
 */
ErrorOrPtr resource_claim(uintptr_t start, size_t size, kresource_type_t type, struct kresource_mirror** regions);

/*
 * Resource release routines
 * Returns:
 * - Success -> the resource is completely free and unreferenced entirely throughout
 * - Warning -> the resource was successfully unreferenced, but parts of it are still referenced
 * - Error   -> something went wrong while trying to release
 */
ErrorOrPtr resource_release_region(struct kresource_mirror** region);
ErrorOrPtr resource_release(uintptr_t start, size_t size, struct kresource_mirror** mirrors_start);

/*
 * This serves as a pseudo-resource in the sense that 
 * it only stores the start address and the size. The
 * resource manager should be able to infer the actual 
 * kresource from this
 * NOTE: the mirror contains the actual address, while the real
 *       resource can contain a lower address. This has to do with 
 *       the fact that we reuse kresource objects when we share them.
 *       owner-sided we should store the absolute address, so we can
 *       do the appropriate arithmatic on it
 */
typedef struct kresource_mirror {
  uintptr_t m_start;
  size_t m_size;
  kresource_type_t m_type;
  kresource_flags_t m_flags;
  struct kresource_mirror* m_next;
} kresource_mirror_t;

#endif // !__ANIVA_SYS_RESOURCE__
