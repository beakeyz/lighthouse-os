#include "resource.h"
#include "dev/debug/serial.h"
#include "libk/bitmap.h"
#include "libk/error.h"
#include "libk/reference.h"
#include "libk/string.h"
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
static zone_allocator_t* __resource_mirror_allocator;

static inline bool __resource_contains(kresource_t* resource, uintptr_t address) 
{
  if (!resource)
    return false;

  return (address >= resource->m_start && address < (resource->m_start + resource->m_size));
}

static inline bool __resource_fits_inside(uintptr_t start, size_t size, kresource_t* resource)
{
  if (!resource)
    return false;

  return (start >= resource->m_start && (start + size) <= (resource->m_start + resource->m_size));
}

static inline bool __covers_entire_resource(uintptr_t start, size_t size, kresource_t* resource)
{
  if (!resource)
    return false;
  return ((start <= resource->m_start) && (start + size >= resource->m_start + resource->m_size)); 
}

/*
 * start and size represent the contained range,
 * the resource is the containing host
 */
static inline ErrorOrPtr __resource_get_low_side_space(uintptr_t start, size_t size, kresource_t* resource)
{
  if (!resource)
    return Error();

  if (!__resource_contains(resource, start))
    return Error();

  if (resource->m_start >= start)
    return Error();

  return Success(start - resource->m_start);
}

/*
 * start and size represent the contained range,
 * the resource is the containing host
 */
static inline ErrorOrPtr __resource_get_high_side_space(uintptr_t start, size_t size, kresource_t* resource)
{
  if (!resource)
    return Error();

  if (!__resource_contains(resource, start + size))
    return Error();

  uintptr_t r_end_addr = resource->m_start + resource->m_size;
  uintptr_t end_addr = start + size;

  if (end_addr > r_end_addr)
    return Error();

  return Success(r_end_addr - end_addr);
}

static inline bool __resource_type_is_valid(kresource_type_t type)
{
  return (type >= KRES_MIN_TYPE && type <= KRES_MAX_TYPE);
}

static bool __resource_is_referenced(kresource_t* resource)
{
  if (resource && resource->m_shared_count >= 1)
    return true;

  return false;
}

static bool __resource_is_shared(kresource_t* resource)
{
  if (!resource)
    return false;

  if (resource->m_shared_count > 1)
    return true;

  if (resource->m_flags & KRES_FLAG_SHARED)
    return true;

  return false;
}

static void __resource_reference(kresource_t* resource)
{
  /* We will be shared with this reference */
  if (resource->m_shared_count)
    resource->m_flags |= KRES_FLAG_SHARED;

  flat_ref(&resource->m_shared_count);
}

static void __resource_unreference(kresource_t* resource)
{
  /* We will be shared with this reference */
  if (resource->m_shared_count <= 1)
    resource->m_flags &= ~KRES_FLAG_SHARED;

  flat_unref(&resource->m_shared_count);
}

static void __resource_reference_range(uintptr_t start, size_t size, kresource_type_t type, kresource_t* r_start)
{
  kresource_t* itt;
  uintptr_t itt_addr;

  if (!__resource_type_is_valid(type))
    return;

  if (!r_start)
    r_start = __resources[type];

  itt = r_start;
  itt_addr = start;

  while (__resource_contains(itt, itt_addr)) {

    __resource_reference(itt);
    
    itt_addr += itt->m_size;

    /* Don't exceed the actual range */
    if (itt_addr >= (start + size)) {
      break;
    }

    itt = itt->m_next;
  }
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
 * Check if a resource is colliding with another resource AND the next
 */
static inline bool __kresource_does_collide_with_next(kresource_t* new, kresource_t* old) 
{
  if (!new || !old || !old->m_next)
    return false;

  if (!__resource_contains(old, new->m_start))
    return false;

  return ((new->m_start + new->m_size) > old->m_next->m_start);
}

static void __destroy_kresource(kresource_t* resource)
{
  memset(resource, 0, sizeof(kresource_t));
  zfree_fixed(__resource_allocator, resource);
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

  ret->m_shared_count = NULL;

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
  bool does_collide;
  bool should_place_after;
  size_t resource_cover_count;
  kresource_t** curr_resource_slot;
  kresource_t* old_resource;
  kresource_mirror_t* new_resource_mirror;

  if (!__resource_mutex || !__resource_allocator || !regions || !__resource_type_is_valid(type))
    return Error();

  mutex_lock(__resource_mutex);

  /* Find the lowest resource that collides with this range */
  curr_resource_slot = __find_kresource(start, type);

  if (!(*curr_resource_slot)) {
    mutex_unlock(__resource_mutex);
    return Error();
  }

  old_resource = *curr_resource_slot;

  /*
   * Now that we have the containing resource, we can determine what to do with it.
   * Let's loop over every single resource that is contained inside the requested resource
   *  - If it's shared, we can increase the reference count
   *  - If it's not shared, we can split off the part that we need.
   *    - If it's not shared and it is entirely enveloped by the requested resource,
   *      we can simply 'claim' it by setting the reference count to 1
   */
  ErrorOrPtr result;
  kresource_t* last_referenced_resource;
  const uintptr_t end_addr = start + size;
  uintptr_t itt_addr = start;
  size_t itt_size = size;

  while (*curr_resource_slot && itt_addr < end_addr) {

    /*
    if (__resource_is_shared(*resource_slot)) {
      __resource_reference(*resource_slot);
      goto cycle;
    }
    */

    /* If the entire resource is covered by the size, reference and go next */
    if (__covers_entire_resource(itt_addr, itt_size, *curr_resource_slot)) {
      println(to_string(start));
      println(to_string(size));
      goto mark_and_cycle;
    }

    /*
     * If we have space left on the low side, that means we'll have to place a new 
     * resource above the old resource
     */
    result = __resource_get_low_side_space(start, size, *curr_resource_slot);

    if (result.m_status == ANIVA_SUCCESS) {

      if (__resource_is_referenced(*curr_resource_slot) && last_referenced_resource != *curr_resource_slot) {
        goto mark_and_cycle;
      }

      uintptr_t resource_start = (*curr_resource_slot)->m_start;
      uintptr_t delta = Release(result);

      kresource_t* split = __create_kresource(resource_start, delta, type);

      (*curr_resource_slot)->m_start += delta;
      (*curr_resource_slot)->m_size  -= delta;

      /* We need to reference this resource here since we are trying to claim the entire range */
      if (last_referenced_resource != *curr_resource_slot)
        __resource_reference(*curr_resource_slot);

      /* Cache */
      last_referenced_resource = *curr_resource_slot;

      split->m_next = *curr_resource_slot;

      /* Place the split-off resource in the link */
      *curr_resource_slot = split;

      /* Set the current slot to the Resource we where able to free and fallthrough */
      curr_resource_slot = &split->m_next;
    }

    /* If we have some space left on the high side, we can split it and claim */
    result = __resource_get_high_side_space(start, size, *curr_resource_slot);

    /*
     * NOTE: Being able to get high space means that 
     * the requested range ends at this resource.
     */
    if (result.m_status == ANIVA_SUCCESS) {

      if (__resource_is_referenced(*curr_resource_slot) && last_referenced_resource != *curr_resource_slot) {
        __resource_reference(*curr_resource_slot);
        break;
      }

      uintptr_t resource_start = (*curr_resource_slot)->m_start;
      uintptr_t resource_end = resource_start + (*curr_resource_slot)->m_size;
      uintptr_t delta = Release(result);
      /* Split is the resource that gets created as a result of the space we have left high */
      kresource_t* split = __create_kresource(resource_end - delta, delta, type);

      (*curr_resource_slot)->m_size  -= delta;

      /* Make sure we don't mark twice */
      if (last_referenced_resource != *curr_resource_slot)
        __resource_reference(*curr_resource_slot);

      split->m_next = (*curr_resource_slot)->m_next;
      (*curr_resource_slot)->m_next = split;

      /* Having space on the high size means we have reached the end of the possible resources we need to handle */
      break;
    }

    if (__resource_is_referenced(*curr_resource_slot) && __resource_contains(*curr_resource_slot, itt_addr)) {
      kernel_panic("referenced entire resource!");
      /* Already marked, just cycle */
      if (last_referenced_resource == *curr_resource_slot)
        goto cycle;
      goto mark_and_cycle;
    }
    
    curr_resource_slot = &(*curr_resource_slot)->m_next;
    continue;
mark_and_cycle:;
    if (last_referenced_resource != *curr_resource_slot)
      __resource_reference(*curr_resource_slot);
    last_referenced_resource = *curr_resource_slot;
cycle:;
    itt_addr += (*curr_resource_slot)->m_size;
    itt_size -= (*curr_resource_slot)->m_size;

    curr_resource_slot = &(*curr_resource_slot)->m_next;
  }

  /* Manually create a new resource mirror */
  new_resource_mirror = zalloc_fixed(__resource_mirror_allocator);

  if (!new_resource_mirror) {
    mutex_unlock(__resource_mutex);
    return Error();
  }

  memset(new_resource_mirror, 0, sizeof(kresource_mirror_t));

  new_resource_mirror->m_start = start;
  new_resource_mirror->m_size = size;
  new_resource_mirror->m_type = type;

  /* Link this mirror into the regions list */
  new_resource_mirror->m_next = *regions;
  *regions = new_resource_mirror;

  mutex_unlock(__resource_mutex);
  return Success(0);
}

ErrorOrPtr resource_release_region(struct kresource_mirror** region) 
{
  uintptr_t start;
  size_t size;
  kresource_type_t type;
  kresource_t* first_used_resource;
  kresource_t* previous_resource;

  if (!region || !*region)
    return Error();

  type = (*region)->m_type;
  start = (*region)->m_start;
  size = (*region)->m_size;

  mutex_lock(__resource_mutex);  

  first_used_resource = __resources[type];
  previous_resource = nullptr;

  /*
   * Find the resource with linear scan
   * TODO: faster algoritm
   */
  while (first_used_resource) {
    if (__resource_contains(first_used_resource, start))
      break;
    previous_resource = first_used_resource;
    first_used_resource = first_used_resource->m_next;
  }

  if (!first_used_resource) {
    mutex_unlock(__resource_mutex);
    return Error();
  }

  /* We now have the first resource that contains our mirror region */

  const uintptr_t end_addr = start + size;

  uintptr_t itter_addr = start;
  size_t itter_size = size;
  kresource_t* itter_resource = first_used_resource;

  while (itter_resource && __resource_contains(itter_resource, itter_addr)) {

    /* Always unreference this entry */
    __resource_unreference(itter_resource);

    /* If both the current and the previous resource are mergable, we can merge them */
    if (previous_resource && itter_resource->m_shared_count == previous_resource->m_shared_count) {
      
      /* First, inflate the previous */
      previous_resource->m_size += itter_resource->m_size;

      /* Second, remove the itter resource from the link */
      previous_resource->m_next = itter_resource->m_next;

      /* Third, free the itter_resource */
      __destroy_kresource(itter_resource);

      /* Last, reset itter_resource */
      itter_resource = previous_resource;
    }

    /* Shift the size */
    itter_addr += itter_resource->m_size;
    itter_size -= itter_resource->m_size;

    previous_resource = itter_resource;
    itter_resource = itter_resource->m_next;
  }

  /* Cache the mirror to remove */
  kresource_mirror_t* mirror = *region;

  /* Skip the mirror in the list */
  *region = (*region)->m_next;

  /* Zero out */
  memset(mirror, 0, sizeof(kresource_mirror_t));

  /* Free */
  zfree_fixed(__resource_mirror_allocator, mirror);

  mutex_unlock(__resource_mutex);

  return Success(0);
}

ErrorOrPtr resource_release(uintptr_t start, size_t size, kresource_mirror_t** mirrors_start)
{

  kresource_mirror_t** current;

  if (!mirrors_start || !*mirrors_start)
    return Error();

  current = mirrors_start;

  /*
   * Find the mirror we care about
   * TODO: switch to better algoritm
   */
  while (current && *current) {
    if ((*current)->m_start == start && (*current)->m_size == size)
      break;

    current = &(*current)->m_next;
  }

  if (!current || !*current)
    return Error();

  return resource_release_region(current);
}

void debug_resources(kresource_type_t type)
{
  uintptr_t index;
  kresource_t* list;

  if (!__resource_type_is_valid(type))
    return;

  list = __resources[type];
  index = 0;

  println("Debugging resources: ");

  if (!__resource_mutex || !list || !__resource_allocator)
    return;

  for (; list; list = list->m_next) {
    print("Resource (");
    print(to_string(index));
    print(") : { ");
    print(to_string(list->m_start));
    print(" (");
    print(to_string(list->m_size));
    print(" bytes) references: ");
    print(to_string(list->m_shared_count));
    println(" } ");
    index ++;
  }
}

void init_kresources() 
{
  __resource_mutex = create_mutex(0);
  __resource_allocator = create_zone_allocator_ex(nullptr, 0, 2 * Mib, sizeof(kresource_t), 0);
  __resource_mirror_allocator = create_zone_allocator_ex(nullptr, 0, 2 * Mib, sizeof(kresource_mirror_t), 0);

  /*
   * Create a base-resource for every resource type that starts
   * at 0 and ends at 64bit_max. Every subsequent resource shall 
   * be spliced from this base
   */
  for (kresource_type_t type = KRES_MIN_TYPE; type <= KRES_MAX_TYPE; type++)
    __resources[type] = __create_kresource(0, 0xFFFFFFFFFFFFFFFF, type);
}
