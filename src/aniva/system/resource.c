#include "resource.h"
#include "entry/entry.h"
#include "libk/flow/error.h"
#include "libk/flow/reference.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
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
static mutex_t* __resource_mutex = nullptr;
static zone_allocator_t* __resource_allocator = nullptr;
static kresource_bundle_t* __kernel_resource_bundle = NULL;

static inline bool __resources_are_mergeable(kresource_t* current, kresource_t* next) 
{
  /* If they are next to eachother AND they have the same referencecount, we can merge them */
  return (current && next && current->m_next == next && current->m_shared_count == next->m_shared_count);
}

/*
static inline bool __resource_fits_inside(uintptr_t start, size_t size, kresource_t* resource)
{
  if (!resource)
    return false;

  return (start >= resource->m_start && (start + size) <= (resource->m_start + resource->m_size));
}
*/

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

  if (!resource_contains(resource, start))
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

  if (!resource_contains(resource, start + size))
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

static void __resource_reference(kresource_t* resource, void* owner)
{
  /*
   * Will we be shared with this reference?
   */
  if (resource->m_shared_count)
    resource->m_flags |= KRES_FLAG_SHARED;

  /* Do we own this resource now? */
  if (!resource->m_owner)
    resource->m_owner = owner;

  flat_ref(&resource->m_shared_count);
}

static void __resource_unreference(kresource_t* resource)
{
  flat_unref(&resource->m_shared_count);

  /* We will be shared with this reference */
  if (resource->m_shared_count <= 1)
    resource->m_flags &= ~KRES_FLAG_SHARED;
  
  /* Remove the owner: we trust that the real resource has been dealt with appropriately */
  if (!resource->m_shared_count)
    resource->m_owner = nullptr;
}
/*
 * Quick fast-slow linkedlist traverse
 * used for binary search
 * TODO: implement in the actual searching stuff
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

    if (fast == end || fast->m_next == end)
      break;

    fast = fast->m_next->m_next;
    slow = slow->m_next;
  }

  return slow;
}
*/

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
    if (resource_contains(*ret, address)) {
      break;
    }
  }

  return ret;
}

/*
 * Check if a resource is colliding with another resource AND the next
static inline bool __kresource_does_collide_with_next(kresource_t* new, kresource_t* old) 
{
  if (!new || !old || !old->m_next)
    return false;

  if (!resource_contains(old, new->m_start))
    return false;

  return ((new->m_start + new->m_size) > old->m_next->m_start);
}
 */

static void __destroy_kresource(kresource_t* resource)
{
  memset(resource, 0, sizeof(kresource_t));
  zfree_fixed(__resource_allocator, resource);
}

static kresource_t* __create_kresource_ex(const char* name, uintptr_t start, size_t size, kresource_type_t type) 
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

  ret->m_name = name;
  ret->m_start = start;
  ret->m_size = size;
  ret->m_type = type;

  ret->m_shared_count = NULL;

  return ret;
}

static kresource_t* __create_kresource(uintptr_t start, size_t size, kresource_type_t type) 
{
  return __create_kresource_ex("Generic resource", start, size, type);
}

/*!
 * @brief Claims a resource for the kernel
 *
 * this can be used for any resources that are used by devices and drivers
 */
ErrorOrPtr resource_claim_kernel(const char* name, void* owner, uintptr_t start, size_t size, kresource_type_t type)
{
  return resource_claim_ex(name, owner, start, size, type, __kernel_resource_bundle);
}

/*!
 * @brief Claim a resource from a pool
 *
 * This routine tries to filter out a new resource object and it adds references to 
 * overlapping resources. @regions needs to be an active object that represents a resource range
 * (Like a memoryspace or an IRQ range)
 */
ErrorOrPtr resource_claim_ex(const char* name, void* owner, uintptr_t start, size_t size, kresource_type_t type, kresource_bundle_t* regions)
{
  kresource_t** curr_resource_slot;

  if (!__resource_mutex || !__resource_allocator || !regions || !__resource_type_is_valid(type))
    return Error();
  
  mutex_lock(__resource_mutex);

  /* Find the lowest resource that collides with this range */
  curr_resource_slot = __find_kresource(start, type, &regions->resources[type]);

  if (!(*curr_resource_slot)) {
    mutex_unlock(__resource_mutex);
    return Error();
  }

  /*
   * Now that we have the containing resource, we can determine what to do with it.
   * Let's loop over every single resource that is contained inside the requested resource
   *  - If it's shared, we can increase the reference count
   *  - If it's not shared, we can split off the part that we need.
   *    - If it's not shared and it is entirely enveloped by the requested resource,
   *      we can simply 'claim' it by setting the reference count to 1
   */
  ErrorOrPtr result;
  kresource_t* last_referenced_resource = nullptr;
  uintptr_t itt_addr = start;
  size_t itt_size = size;

  while (*curr_resource_slot && itt_addr < (start + size)) {

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
        __resource_reference(*curr_resource_slot, owner);

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
        __resource_reference(*curr_resource_slot, owner);
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
        __resource_reference(*curr_resource_slot, owner);

      split->m_next = (*curr_resource_slot)->m_next;

      (*curr_resource_slot)->m_next = split;

      /* Having space on the high size means we have reached the end of the possible resources we need to handle */
      break;
    }

    if (__resource_is_referenced(*curr_resource_slot) && resource_contains(*curr_resource_slot, itt_addr)) {
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
      __resource_reference(*curr_resource_slot, owner);
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

  /* We can't live without a current_region =/ */
  if (!current_resource)
    return Error();

  /* Grab region info */
  start = current_resource->m_start;
  size = current_resource->m_size;

  /* Lock */
  mutex_lock(__resource_mutex);  

  /* Prepare itterator varialbles */
  uintptr_t itter_addr = start;
  size_t itter_size = size;
  kresource_t* itter_resource = current_resource;

  while (itter_resource && itter_size && resource_contains(itter_resource, itter_addr)) {

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
    if (resource_contains(current, start))
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


/*
 * Looks for a unused virtual region of the specified size we can 
 * Creates a kresource mirror that may be used in order to 
 * claim this region, for instance
 */
ErrorOrPtr resource_find_usable_range(kresource_bundle_t* bundle, kresource_type_t type, size_t size)
{
  kresource_t* current;

  if (!bundle)
    return Error();

  current = bundle->resources[type];

  if (!current)
    return Error();

  do {
    /* Unused resource? */
    if (!current->m_shared_count && current->m_size >= size)
      return Success(current->m_start);

  } while ((current = current->m_next) != nullptr);

  return Error();
}

void destroy_kresource(kresource_t* resource)
{
  memset(resource, 0, sizeof(kresource_t));
  zfree_fixed(__resource_allocator, resource);
}

kresource_bundle_t* create_resource_bundle(page_dir_t* dir)
{
  kresource_bundle_t* ret;

  ret = kmalloc(sizeof(*ret));

  if (!ret)
    return nullptr;

  /*
   * Create a base-resource for every resource type that starts
   * at 0 and ends at 64bit_max. Every subsequent resource shall 
   * be spliced from this base
   */
  for (kresource_type_t type = KRES_MIN_TYPE; type <= KRES_MAX_TYPE; type++) {
    kresource_t* start_resource = __create_kresource(0, 0xFFFFFFFFFFFFFFFF, type);

    ret->resources[type] = start_resource;
  }

  ret->page_dir = dir;

  return ret;
}

static void __clear_mem_resource(kresource_t* resource, kresource_bundle_t* bundle)
{
  uint64_t start = resource->m_start;
  uint64_t size = resource->m_size;

  //printf("(%s) Start: 0x%llx, Size: 0x%llx, dir: %p\n", resource->m_name, start, resource->m_size, bundle->page_dir);

  if ((resource->m_flags & KRES_FLAG_MEM_KEEP_PHYS) != KRES_FLAG_MEM_KEEP_PHYS)
    __kmem_dealloc_ex(bundle->page_dir ? bundle->page_dir->m_root : NULL, bundle, start, size, false, true, true);
}

/*!
 * @brief Loops over the resources in the bundle and cleans them up
 *
 * Every type of resource needs its own type of cleanup, which is why we loop over every
 * used resource to release it. We should however (TODO again) try to support resources 
 * managing their teardown themselves, since we can simply give them a fuction pointer to
 * a function that knows how to destroy and cleanup that type of resource (like kmalloc, vs __kmem_kernel_alloc)
 */
static void __bundle_clear_resources(kresource_bundle_t* bundle)
{
  kresource_t* start_resource;
  kresource_t* current;
  kresource_t* next;

  for (kresource_type_t type = 0; type < KRES_MAX_TYPE; type++) {

    /* Find the first resource of this type */
    current = start_resource = bundle->resources[type];

    while (current) {

      next = current->m_next;

      size_t size = current->m_size;

      /* Skip mirrors with size zero or no references */
      if (!size || !current->m_shared_count)
        goto next_resource;

      /* TODO: destry other resource types */
      switch (current->m_type) {
        case KRES_TYPE_MEM:
          __clear_mem_resource(current, bundle);
          break;
        default:
          /* Skip this entry for now */
          break;
      }

next_resource:
      if (current->m_next) {
        ASSERT_MSG(current->m_type == current->m_next->m_type, "Found type mismatch while cleaning process resources");
      }

      current = next;
      continue;
    }
  }
}

/*!
 * @brief Clears all the resources in a bundle that we own
 * Nothing to add here...
 */
ErrorOrPtr resource_clear_owned(void* owner, kresource_type_t type, kresource_bundle_t* bundle)
{
  kresource_t* next;
  kresource_t* current;

  /* We cannot clear anonymus resources with this */
  if (!owner)
    return Error();

  next = GET_RESOURCE(bundle, type);

  if (!next)
    return Error();

  while (next) {
    current = next;
    next = next->m_next;

    /* Only clear if we own this resource */
    if (current->m_owner == owner)
      __clear_mem_resource(current, bundle);
  }

  return Success(0);
}

void destroy_resource_bundle(kresource_bundle_t* bundle)
{
  kresource_t* current;

  __bundle_clear_resources(bundle);
  
  for (kresource_type_t type = KRES_MIN_TYPE; type <= KRES_MAX_TYPE; type++) {
    
    current = bundle->resources[type];

    while (current) {

      kresource_t* next = current->m_next;

      __destroy_kresource(current);

      current = next;
    }
  }

  kfree(bundle);
}

void init_kresources() 
{
  __resource_mutex = create_mutex(0);

  /* Seperate the mirrors from the system kresources */
  __resource_allocator = create_zone_allocator_ex(nullptr, 0, 128 * Kib, sizeof(kresource_t), 0);

  __kernel_resource_bundle = create_resource_bundle(NULL);

  g_system_info.sys_flags |= SYSFLAGS_HAS_RMM;
}
