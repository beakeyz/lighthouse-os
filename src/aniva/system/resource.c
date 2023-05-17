#include "resource.h"
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

static kernel_resource_t** __find_kernel_resource(kernel_resource_t* resource);
static kernel_resource_t** __find_kernel_resource_by_name(kernel_resource_type_t r_type, char* r_name);

static void __insert_kernel_resource(kernel_resource_t* resource);
static void __remove_kernel_resource(kernel_resource_t* resource);

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
