#include "handle.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "sync/mutex.h"
#include <dev/driver.h>
#include <proc/proc.h>
#include <mem/kmem_manager.h>

/*
 * Called when a handle changes to a new type
 */
static void __on_handle_change_to(khandle_t handle)
{
  switch (handle.type) {
    case KHNDL_TYPE_DRIVER:
      handle.reference.driver->m_flags |= DRV_HAS_HANDLE;
      break;
    case KHNDL_TYPE_PROC:
      handle.reference.process->m_flags |= PROC_HAD_HANDLE;
      break;
    case KHNDL_TYPE_FILE:
    case KHNDL_TYPE_FS_ROOT:
    case KHNDL_TYPE_KOBJ:
    case KHNDL_TYPE_VOBJ:
    case KHNDL_TYPE_NONE:
    default:
      break;
  }
}

void create_khandle(khandle_t* out_handle, khandle_type_t* type, void* ref)
{

  if (!ref)
    return;

  memset(out_handle, 0, sizeof(khandle_t));

  out_handle->index = KHNDL_INVALID_INDEX;
  out_handle->type = *type;
  out_handle->reference.kobj = ref;

  /*
   * Update for the 'type change'
   */
  __on_handle_change_to(*out_handle);
}

/*
 * Called when a handle changes away from its 'old' type 
 */
static void __on_handle_change_from(khandle_t handle)
{
  switch (handle.type) {
    case KHNDL_TYPE_DRIVER:
      handle.reference.driver->m_flags &= ~DRV_HAS_HANDLE;
      break;
    case KHNDL_TYPE_PROC:
      handle.reference.process->m_flags &= ~PROC_HAD_HANDLE;
      break;
    case KHNDL_TYPE_FILE:
    case KHNDL_TYPE_FS_ROOT:
    case KHNDL_TYPE_KOBJ:
    case KHNDL_TYPE_VOBJ:
    case KHNDL_TYPE_NONE:
    default:
      break;
  }
}

void destroy_khandle(khandle_t* handle) 
{
  if (!handle)
    return;

  __on_handle_change_from(*handle);

  memset(handle, 0, sizeof(khandle_t));
}

/*
 * Check if the handle seems bound by looking for its index and reference
 */
static bool __is_khandle_bound(khandle_t* handle) 
{
  return (handle != nullptr && ((handle->index != KHNDL_INVALID_INDEX) && handle->reference.kobj != nullptr));
}

khandle_map_t create_khandle_map_ex(uint32_t max_count)
{
  khandle_map_t ret;
  
  if (!max_count)
    max_count = KHNDL_DEFAULT_ENTRIES;

  memset(&ret, 0, sizeof(khandle_map_t));

  ret.max_count = max_count;
  ret.lock = create_mutex(0);

  /* Ensure we can create this list that provides a maximum of 1024 handles */
  ret.handles = (khandle_t*)Must(__kmem_kernel_alloc_range(max_count * sizeof(khandle_t), NULL, NULL));

  /* Zero */
  memset(ret.handles, 0, SMALL_PAGE_SIZE);

  /* Should we? */
  for (uint32_t i = 0; i < ret.max_count; i++) {
    ret.handles[i].index = KHNDL_INVALID_INDEX;
  }

  return ret;
}

/* Cleans up the maps handles */
void destroy_khandle_map(khandle_map_t* map)
{
  if (!map)
    return;

  destroy_mutex(map->lock);

  Must(__kmem_kernel_dealloc((vaddr_t)map->handles, SMALL_PAGE_SIZE));
}

khandle_t* find_khandle(khandle_map_t* map, uint32_t user_handle)
{
  if (!map)
    return nullptr;

  if (!__is_khandle_bound(&map->handles[user_handle]))
    return nullptr;

  return &map->handles[user_handle];
}

static ErrorOrPtr __try_reallocate_handles(khandle_map_t* map, size_t new_max_count)
{
  khandle_t* new_list;

  /* Debug */
  kernel_panic("TODO: had to reallocate khandles! verify this works (__try_reallocate_handles)");

  if (!map || !new_max_count || new_max_count > KHNDL_MAX_ENTRIES)
    return Error();

  /* Allocate new space */
  TRY(reallocate_result, __kmem_kernel_alloc_range(new_max_count * sizeof(khandle_t), NULL, NULL));

  /* Set the pointer */
  new_list = (khandle_t*)reallocate_result;

  /* Since they are stored as one massive vector, we can simply copy all of it over */
  memcpy(map->handles, new_list, map->max_count * sizeof(khandle_t));

  /* We can now deallocate the old list */
  TRY(deallocate_result, __kmem_kernel_dealloc((uint64_t)map->handles, map->max_count * sizeof(khandle_t)));

  /* Update max_count */
  map->max_count = new_max_count;

  return Success(0);
}

static void __bind_khandle(khandle_map_t* map, khandle_t* handle, uint32_t index)
{
  if (!map || !handle)
    return;
  
  /* First mutate the index */
  handle->index = index;

  /* Then copy it over */
  map->handles[index] = *handle;
  map->count++;
}

/* NOTE: mutates the handle to fill in the index they are put at */
ErrorOrPtr bind_khandle(khandle_map_t* map, khandle_t* handle) 
{

  ErrorOrPtr res;
  uint32_t current_index;
  khandle_t* slot;

  if (!map || !handle)
    return Error();

  mutex_lock(map->lock);

  if (map->count >= map->max_count) {

    /* Have we reached the hardlimit? */
    if (map->max_count >= KHNDL_MAX_ENTRIES)
      return Error();

    /* __try_reallocate_handles will mutate the values in map */
    res = __try_reallocate_handles(map, KHNDL_MAX_ENTRIES);

    /* If we fail to reallocate, thats kinda cringe lol */
    if (IsError(res)) {
      mutex_unlock(map->lock);
      return res;
    }
  }

  /* Check if the next free index is valid */
  current_index = map->next_free_index;
  slot = &map->handles[current_index];

  if (!__is_khandle_bound(slot)) {

    /* insert into array */
    __bind_khandle(map, handle, current_index); 

    map->next_free_index += 1;
    goto unlock_and_success;
  }

  /* Check if we can put it somewhere in a hole */
  current_index = 0;

  /* Loop over the entire array */
  while (current_index < map->max_count) {

    slot = &map->handles[current_index];

    if (!__is_khandle_bound(slot)) {

      __bind_khandle(map, handle, current_index); 

      /* Make sure that the next free index is still valid */
      map->next_free_index = current_index + 1;
      goto unlock_and_success;
    }

    current_index++;
  }

  /* Could not find a place to bind this handle, abort */
  mutex_unlock(map->lock);
  return Error();

unlock_and_success:
  mutex_unlock(map->lock);
  return Success(current_index);
}

/* NOTE: mutates the handle to clear the index */
ErrorOrPtr unbind_khandle(khandle_map_t* map, khandle_t* handle) 
{
  if (!map || !handle)
    return Error();

  /* Zero out this entry */
  destroy_khandle(&map->handles[handle->index]);

  map->next_free_index = handle->index;

  /* Set the correct index */
  handle->index = KHNDL_INVALID_INDEX;

  return Success(0);
}

ErrorOrPtr mutate_khandle(khandle_map_t* map, khandle_t handle, uint32_t index) 
{
  if (!map)
    return Error();

  /* Make sure we don't yeet away the index */
  handle.index = index;

  if (index >= map->max_count)
    return Error();

  __on_handle_change_from(map->handles[index]);

  map->handles[index] = handle;

  return Success(0);
}
