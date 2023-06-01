#include "resource.h"
#include "libk/bitmap.h"
#include "libk/error.h"
#include "mem/heap.h"
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

static mutex_t* __g_resource_lock;

static kernel_resource_t* __mem_range_resources;
static kernel_resource_t* __io_range_resources;
static kernel_resource_t* __irq_resources;

static size_t __mem_range_count;
static size_t __io_range_count;
static size_t __irq_count;

static kernel_resource_t** __resource_table[] = {
  [RESOURCE_TYPE_MEMORY] =           &__mem_range_resources,
  [RESOURCE_TYPE_IO_RANGE] =            &__io_range_resources,
  [RESOURCE_TYPE_IRQ] =                 &__irq_resources,
};

static size_t* __count_table[] = {
  [RESOURCE_TYPE_MEMORY] =           &__mem_range_count,
  [RESOURCE_TYPE_IO_RANGE] =            &__io_range_count,
  [RESOURCE_TYPE_IRQ] =                 &__irq_count,
};

static void __increment_resource_count(kernel_resource_type_t type)
{
  (*__count_table[type])++;
}

static void __decrement_resource_count(kernel_resource_type_t type) 
{
  (*__count_table[type])--;
}

/*
 * We find the appropriate list by using the type enum as an index into
 * an array we have predefined above
 */
static kernel_resource_t** __find_kernel_resource(kernel_resource_t* resource) 
{

  if (!resource)
    return nullptr;

  kernel_resource_t** ret;

  for (ret = __resource_table[resource->m_type]; *ret; ret = &(*ret)->m_next) {
    if (*ret == resource) {
      break;
    }
  }

  return ret;
}

static kernel_resource_t** __find_kernel_resource_by_name(kernel_resource_type_t r_type, char* r_name) 
{

  if (!r_name)
    return nullptr;

  kernel_resource_t** ret;

  for (ret = __resource_table[r_type]; *ret; ret = &(*ret)->m_next) {
    if (memcmp((*ret)->m_name, r_name, strlen(r_name))) 
      break;
  }

  return ret;
}

/*
 * Locks the global lock and inserts a resource into the list
 */
static ErrorOrPtr __insert_kernel_resource(kernel_resource_t* resource) 
{
  
  if (!resource)
    return Error();

  /* Lock the global lock */
  mutex_lock(__g_resource_lock);

  kernel_resource_t** slot = __find_kernel_resource(resource);

  if (!slot)
    goto error_and_exit;

  if (*slot) 
    goto error_and_exit;

  *slot = resource;

  __increment_resource_count(resource->m_type);

  /* Unlock before returning success */
  mutex_unlock(__g_resource_lock);

  return Success(0);

error_and_exit:
  mutex_unlock(__g_resource_lock);

  return Error();
}

/*
 * Locks the global lock and remove a resource from the list
 */
static ErrorOrPtr __remove_kernel_resource(kernel_resource_t* resource) 
{

  kernel_resource_t** itterator;
  
  if (!resource)
    return Error();

  /* Lock the global lock */
  mutex_lock(__g_resource_lock);

  /* Zero */
  itterator = __resource_table[resource->m_type];

  while (*itterator) {
    if (*itterator == resource) {

      *itterator = resource->m_next;
      resource->m_next = nullptr;

      mutex_unlock(__g_resource_lock);

      return Success(0);
    }

    itterator = &(*itterator)->m_next;
  }

  mutex_unlock(__g_resource_lock);
  return Error();
}

/*
 * Memory resource
 *
 * This resource should be able to do a bunch of things:
 *  - Devide itself into ranges 
 *  - Know about its users
 *  - Know about its mappings
 *  - Make sure certain rules are followed that ensure the resource 
 *    is not used by anyone but its owners
 */

memory_resource_data_t* __create_memory_resource_data(page_dir_t* dir, memory_resource_type_t mem_type)
{

  size_t _types_size;
  memory_resource_data_t* data = kmalloc(sizeof(memory_resource_data_t));

  if (!data)
    return nullptr;

  memset(data, 0, sizeof(memory_resource_data_t));

  data->m_type = mem_type;
  data->m_dir = dir;
  data->m_flags = 0;

  _types_size = sizeof(void*) * MEMORY_RESOURCE_TYPE_COUNT;

  data->m_types = kmalloc(_types_size);
  memset(data->m_types, 0, _types_size);

  data->m_phys_bitmap = nullptr;
  data->m_highest_phys_idx = 0;
  data->m_lowest_phys_idx = 0;

  return data;
}

ErrorOrPtr __destroy_memory_resource_data(memory_resource_data_t* data) 
{

  if (data->m_phys_bitmap) {

    /* TODO: free the pages that the bitmap specifies */
    if (data->m_lowest_phys_idx && data->m_highest_phys_idx)
      kernel_panic("TODO: implement physical page management in __destroy_memory_resource_data");

    destroy_bitmap(data->m_phys_bitmap);
  }

  for (uintptr_t i = 0; i < MEMORY_RESOURCE_TYPE_COUNT; i++) {
    kernel_resource_t* child = data->m_types[i];

    if (!child)
      continue;

    TRY(_, child->m_ops->f_destroy(child));
  }

  kfree(data->m_types);

  kfree(data);

  return Success(0);
}

ErrorOrPtr __destroy_memory_resource(kernel_resource_t* resource)
{

  if (!resource)
    return Error();

  TRY(_, __destroy_memory_resource_data(resource->m_resource_specific_data));

  destroy_mutex(resource->m_lock);

  kfree(resource->m_name);
  kfree(resource);

  return Success(0);
}

kernel_resource_ops_t mem_range_ops = {
  .f_destroy = __destroy_memory_resource,
  // TODO: 
};

kernel_resource_t* create_memory_resource(char* name, vaddr_t virtual_base, size_t length, page_dir_t* dir, memory_resource_type_t mem_type)
{
  if (!name || !length)
    return nullptr;

  kernel_resource_t* resource = kmalloc(sizeof(kernel_resource_t));

  if (!resource)
    return nullptr;

  memset(resource, 0, sizeof(kernel_resource_t));

  resource->m_type = RESOURCE_TYPE_MEMORY;

  resource->m_name = kmalloc(strlen(name) + 1);
  memcpy(resource->m_name, name, strlen(name));
  resource->m_name[strlen(name)] = NULL;

  resource->m_ops = &mem_range_ops;

  resource->m_lock = create_mutex(0);

  resource->m_data = virtual_base;
  resource->m_length = length;

  resource->m_owner_type = RESOURCE_OWNER_TYPE_NONE;

  resource->m_resource_specific_data = __create_memory_resource_data(dir, mem_type);

  if (!resource->m_resource_specific_data)
    goto fail_and_exit;

  return resource;

fail_and_exit:

  destroy_mutex(resource->m_lock);
  kfree(resource->m_name);
  kfree(resource);

  return nullptr;
}


kernel_resource_ops_t io_range_ops = {

};

kernel_resource_ops_t irq_ops = {

};

/*
 * NOTE: kernel resources require the initial kernel heap
 */
void init_kernel_resources() 
{

  /* Create mutex for global resource mutation */
  __g_resource_lock = create_mutex(0);

  /* Zero out the 'global' variables */
  __mem_range_count = 0;
  __io_range_count = 0;
  __irq_count = 0;

  __mem_range_resources = nullptr;
  __io_range_resources = nullptr;
  __irq_resources = nullptr;

}
