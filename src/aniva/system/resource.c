#include "resource.h"
#include "dev/debug/serial.h"
#include "libk/data/bitmap.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/zalloc.h"
#include "sync/mutex.h"

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
//static kresource_t* __resources[3];
static mutex_t* __resource_mutex;
static zone_allocator_t* __resource_allocator;
//static zone_allocator_t* __resource_mirror_allocator;

static inline bool __resource_contains(kresource_t* resource, uintptr_t address) 
{
  if (!resource)
    return false;

  return (address >= resource->m_start && address < (resource->m_start + resource->m_size));
}

static inline bool __resources_are_mergeable(kresource_t* current, kresource_t* next) 
{
  /* If they are next to eachother AND they have the same referencecount, we can merge them */
  return (current && next && current->m_next == next && current->m_shared_count == next->m_shared_count);
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

  //ASSERT_MSG(r_start, "Tried to reference a resource range, but none was given");

  if (!r_start)
    return;

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

  if (!start)
    return nullptr;

  fast = start;
  end = start;

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
static kresource_t** __find_kresource(uintptr_t address, kresource_type_t type, kresource_t** start)
{
  kresource_t** ret;

  if (!__resource_type_is_valid(type))
    return nullptr;

  /* Find the appropriate list for this type */
  ret = start;

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

static kresource_t* __create_kresource_ex(const char* name, uintptr_t start, size_t size, kresource_type_t type) 
{
  kresource_t* ret;
  size_t name_len;

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

  name_len = strlen(name);

  if (name_len >= sizeof(ret->m_name))
    name_len = sizeof(ret->m_name) - 1;

  memcpy((uint8_t*)ret->m_name, name, name_len);

  ret->m_start = start;
  ret->m_size = size;
  ret->m_type = type;

  ret->m_shared_count = NULL;

  return ret;
}

static kresource_t* __create_kresource(uintptr_t start, size_t size, kresource_type_t type) 
{
  return __create_kresource_ex("Generic claim", start, size, type);
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

ErrorOrPtr resource_claim(uintptr_t start, size_t size, kresource_t** regions)
{
  kresource_type_t type;

  if (!regions || !(*regions))
    return Error();

  type = (*regions)->m_type;
  
  return resource_claim_ex("Generic Claim", start, size, type, regions);
}

ErrorOrPtr resource_claim_ex(const char* name, uintptr_t start, size_t size, kresource_type_t type, kresource_t** regions)
{
  bool does_collide;
  bool should_place_after;
  size_t resource_cover_count;
  kresource_t** curr_resource_slot;
  kresource_t* old_resource;
  kresource_t* new_mirror;

  if (!__resource_mutex || !__resource_allocator || !regions || !(*regions) || !__resource_type_is_valid(type))
    return Error();

  if ((*regions)->m_type != type)
    return Error();

  mutex_lock(__resource_mutex);

  /* Find the lowest resource that collides with this range */
  curr_resource_slot = __find_kresource(start, type, regions);

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
  size_t name_len;

  while (*curr_resource_slot && itt_addr < end_addr) {

    /*
    if (__resource_is_shared(*resource_slot)) {
      __resource_reference(*resource_slot);
      goto cycle;
    }
    */

    /* If the entire resource is covered by the size, reference and go next */
    if (__covers_entire_resource(itt_addr, itt_size, *curr_resource_slot)) {
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
      //kernel_panic("referenced entire resource!");
      /* Already marked, just cycle */
      if (last_referenced_resource == *curr_resource_slot)
        goto cycle;
      goto mark_and_cycle;
    }
    
    curr_resource_slot = &(*curr_resource_slot)->m_next;
    continue;

    /* We can mark this resource and go next */
mark_and_cycle:;
    if (last_referenced_resource != *curr_resource_slot) {
      __resource_reference(*curr_resource_slot);
    }
    last_referenced_resource = *curr_resource_slot;

    /* Mutate stats and go next */
cycle:;
    itt_addr += (*curr_resource_slot)->m_size;
    itt_size -= (*curr_resource_slot)->m_size;

    curr_resource_slot = &(*curr_resource_slot)->m_next;

  }

  mutex_unlock(__resource_mutex);
  return Success(0);
}

/* 
 * NOTE: Since a releaseable region will always have 
 */
ErrorOrPtr resource_release_region(kresource_t* previous_resource, kresource_t* current_resource)
{
  uintptr_t start;
  size_t size;
  kresource_type_t type;

  /* We can't live without a current_region =/ */
  if (!current_resource)
    return Error();

  /* Grab region info */
  type = current_resource->m_type;
  start = current_resource->m_start;
  size = current_resource->m_size;

  /* Lock */
  mutex_lock(__resource_mutex);  

  /* Compute end address */
  const uintptr_t end_addr = start + size;

  /* Prepare itterator varialbles */
  uintptr_t itter_addr = start;
  size_t itter_size = size;
  kresource_t* itter_resource = current_resource;

  while (itter_resource && itter_size && __resource_contains(itter_resource, itter_addr)) {

    /* Always unreference this entry */
    __resource_unreference(itter_resource);

    /* If both the current and the previous resource are mergable, we can merge them */
    if (previous_resource && itter_resource->m_shared_count == previous_resource->m_shared_count) {
      
      /*
       * Before we do anything, make sure the itterator size is buffered 
       * correctly
       */
      itter_size += previous_resource->m_size;

      /* Also include the next resource, if its mergable */
      if (__resources_are_mergeable(itter_resource, itter_resource->m_next))
        itter_size += itter_resource->m_next->m_size;

      /* First, inflate the previous */
      previous_resource->m_size += itter_resource->m_size;

      /* Second, remove the itter resource from the link */
      previous_resource->m_next = itter_resource->m_next;

      /* Third, free the itter_resource */
      __destroy_kresource(itter_resource);

      /* Last, reset itter_resource */
      itter_resource = previous_resource;
    }

    /* Shift the addres and size */
    itter_addr += itter_resource->m_size + 1;
    itter_size -= (itter_resource->m_size >= itter_size ? itter_resource->m_size : itter_size);

    previous_resource = itter_resource;
    itter_resource = itter_resource->m_next;
  }

  /* Also try to merge with any resources that are next to us */
  itter_resource = previous_resource;

  while (itter_resource && __resources_are_mergeable(itter_resource, itter_resource->m_next)) {

    kresource_t* next = itter_resource->m_next;

    /* Inflate */
    itter_resource->m_size += next->m_size;

    /* Decouple */
    itter_resource->m_next = next->m_next;

    /* Destroy */
    __destroy_kresource(next);
  }

  mutex_unlock(__resource_mutex);
  return Success(0);
}

ErrorOrPtr resource_release(uintptr_t start, size_t size, kresource_t* mirrors_start)
{
  ErrorOrPtr result;
  kresource_t* current;
  kresource_t* previous;
  kresource_t* next;

  /* No */
  if (!size)
    return Error();

  next = nullptr;
  previous = nullptr;
  current = mirrors_start;

  /*
   * Find the mirror we care about
   * TODO: switch to better algoritm
   */
  while (current) {
    if (__resource_contains(current, start))
      break;

    previous = current;
    current = current->m_next;
  }

  /* This is the only resource we need to release, let's do it! */
  if (__covers_entire_resource(start, size, current)) {
    return resource_release_region(previous, current);
  }

  /*
   * address is in the current resource, but we can't find a resource that 
   * satisfied this release request. This means that we are spread over multiple
   * resource objects...
   */
  result = Error();

  while (size && current) {

    if (current->m_size >= size)
      size -= current->m_size;
    else
      size = NULL;

    next = current->m_next;

    result = resource_release_region(previous, current);

    if (IsError(result))
      break;

    current = next;
  }

  return result;
}

ErrorOrPtr resource_apply_flags(uintptr_t start, size_t size, kresource_flags_t flags, kresource_t* regions)
{
  kresource_t** start_resource;
  kresource_t* itt;
  uintptr_t itt_addr;

  if (!size || !regions)
    return Error();

  start_resource = __find_kresource(start, regions->m_type, &regions);

  if (!start_resource || !*start_resource)
    return Error();

  mutex_lock(__resource_mutex);

  itt = *start_resource;
  itt_addr = start;

  while (itt && itt_addr < (start + size)) {
    itt->m_flags |= flags;

    itt_addr += itt->m_size;

    itt = itt->m_next;
  }

  mutex_unlock(__resource_mutex);
  return Success(0);
}

void debug_resources(kresource_t** bundle, kresource_type_t type)
{
  uintptr_t index;
  kresource_t* list;

  if (!__resource_type_is_valid(type))
    return;

  list = bundle[type];
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

void destroy_kresource(kresource_t* resource)
{
  memset(resource, 0, sizeof(kresource_t));
  zfree_fixed(__resource_allocator, resource);
}

void create_resource_bundle(kresource_bundle_t* out)
{
  /*
   * Create a base-resource for every resource type that starts
   * at 0 and ends at 64bit_max. Every subsequent resource shall 
   * be spliced from this base
   */
  for (kresource_type_t type = KRES_MIN_TYPE; type <= KRES_MAX_TYPE; type++) {
    kresource_t* start_resource = __create_kresource(0, 0xFFFFFFFFFFFFFFFF, type);

    (*out)[type] = start_resource;
  }
}

void destroy_resource_bundle(kresource_bundle_t bundle)
{
  kresource_t* current;
  
  for (kresource_type_t type = KRES_MIN_TYPE; type <= KRES_MAX_TYPE; type++) {
    
    current = bundle[type];

    while (current) {

      kresource_t* next = current->m_next;

      __destroy_kresource(current);

      current = next;
    }
  }
}

void init_kresources() 
{
  __resource_mutex = create_mutex(0);

  /* Seperate the mirrors from the system kresources */
  __resource_allocator = create_zone_allocator_ex(nullptr, 0, 128 * Kib, sizeof(kresource_t), 0);
  //__resource_mirror_allocator = create_zone_allocator_ex(nullptr, 0, 128 * Kib, sizeof(kresource_t), 0);

}
