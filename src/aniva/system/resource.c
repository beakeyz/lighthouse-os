#include "resource.h"
#include "libk/bitmap.h"
#include "libk/error.h"
#include "mem/heap.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"
#include <string.h>

/*
 * NOTE: in this file (or range of files) I try to enforce a different 
 * code-style than I have been using than in the rest of this project.
 * at this point there have been a lot of changes in style, but that is 
 * mainly due to the fact that I've been working on this for a while now,
 * and people simply change
 */

/*
 * Here we need to do a few things:
 *  - Keep track of the resources that the kernel is aware of
 *  - Make sure resources don't overlap
 *  - Make sure only one kernelobject owns a given resource
 *  - Make sure resources are grouped and ordered nicely so we can easly and quickly find them
 */
static kresource_t* __resources[3];
static mutex_t* __resource_mutex;
static zone_allocator_t* __resource_allocator;

static inline bool __resource_contains(kresource_t* resource, uintptr_t address) 
{
  return (address >= resource->m_start && address < resource->m_size);
}

static inline bool __resource_type_is_valid(kresource_type_t type)
{
  return (type >= KRES_MIN_TYPE && type <= KRES_MAX_TYPE);
}

static bool __resource_is_shared(kresource_t* resource)
{
  if (!resource)
    return false;

  if (resource->m_shared_count.m_count > 1)
    return true;

  if (resource->m_flags & KRES_FLAG_SHARED)
    return true;

  return false;
}

/*
 * Quick fast-slow linkedlist traverse
 * used for binary search
 */
static kresource_t* __find_middle_kresource(kresource_t* start, kresource_t* end, kresource_type_t type)
{
  kresource_t* fast;
  kresource_t* slow;

  if (!__resource_type_is_valid(type))
    return nullptr;

  fast = start;
  end = start;

  if (!fast)
    fast = __resources[type];

  while (fast && fast->m_next) {

    if (fast == end)
      break;

    fast = fast->m_next->m_next;
    slow = slow->m_next;
  }

  return slow;
}

/*
 * TODO: switch to binary search to account for large resource counts
 */
static kresource_t** __find_kresource(uintptr_t address, kresource_type_t type)
{
  kresource_t** ret;

  if (!__resource_type_is_valid(type))
    return nullptr;

  /* Find the appropriate list for this type */
  ret = &__resources[type];

  for (; *ret; ret = &(*ret)->m_next) {
    if (__resource_contains(*ret, address)) 
      break;
  }

  return ret;
}

/*
 * TODO: switch to binary search to account for large resource counts
 */
static kresource_t** __find_prev_kresource(kresource_t* source, kresource_type_t type)
{
  kresource_t** ret;
  kresource_t** itt;

  if (!__resource_type_is_valid(type))
    return nullptr;

  /* Find the appropriate list for this type */
  itt = &__resources[type];
  ret = nullptr;

  for (; *itt; itt = &(*itt)->m_next) {
    if (*itt == source) 
      break;

    ret = itt;
  }

  return ret;
}

static kresource_t* __create_kresource(uintptr_t start, size_t size, kresource_type_t type) 
{
  kresource_t* ret;

  /* Allocator should be initialized here */
  if (!__resource_allocator)
    return nullptr;

  /* We can't create a resource with an invalid type */
  if (!__resource_type_is_valid(type))
    return nullptr;

  ret = zalloc_fixed(__resource_allocator);

  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(kresource_t));

  ret->m_start = start;
  ret->m_size = size;
  ret->m_type = type;

  ret->m_shared_count.m_referenced_handle = ret;

  return ret;
}

/*
 * Create a new allocation and copy the source into it
 */
static kresource_t* __copy_kresource(kresource_t source)
{
  kresource_t* ret = zalloc_fixed(__resource_allocator);

  if (!ret)
    return nullptr;

  memcpy(ret, &source, sizeof(kresource_t));

  /* Clear the next ptr to prevent confusion */
  ret->m_next = nullptr;

  return ret;
}

ErrorOrPtr resource_claim(uintptr_t start, size_t size, kresource_type_t type, struct kresource_mirror** regions)
{
  kresource_t* containing_resource;

  if (!regions || !__resource_type_is_valid(type))
    return Error();

  containing_resource = *__find_kresource(start, type);

  if (!containing_resource)
    return Error();

  /* Now that we have the containing resource, we can determine what to do with it */
  if (__resource_is_shared(containing_resource)) {
    /* Oof, we'll have to share this resource */

    /* Is this shareable? */

    return Success(0);
  }

  /* Split off a new resource */

  kernel_panic("Nein");
}

void init_kresources() 
{
  __resource_mutex = create_mutex(0);
  __resource_allocator = create_zone_allocator_ex(nullptr, 0, 2 * Mib, sizeof(kresource_t), 0);

  /*
   * Create a base-resource for every resource type that starts
   * at 0 and ends at 64bit_max. Every subsequent resource shall 
   * be spliced from this base
   */
  for (kresource_type_t type = KRES_MIN_TYPE; type <= KRES_MAX_TYPE; type++)
    __resources[type] = __create_kresource(0, 0xFFFFFFFFFFFFFFFF, type);
}
