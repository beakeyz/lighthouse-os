#ifndef __ANIVA_RESOURCE_LIST__
#define __ANIVA_RESOURCE_LIST__


#include "system/resource.h"


/*
 * The resource shopping cart
 *
 * This struct manages a bunch of (maybe unrelated) resources in a list.
 * The list can be only of one resource type and is sorted from low to big
 * (meaning that low resource addresses go at the start of the link and high
 * addresses go at the end of the link)
 *
 * This struct is mostly used for device drivers that must handel devices which 
 * reserve certain memory ranges
 */
typedef struct resource_list {
  const char* name;
  kresource_type_t type;

  uint32_t count;

  /* The first resource (also the smalest) */
  kresource_t* start;
  /* The last resource (which has the biggest base address) */
  kresource_t* end;
} resource_list_t;

void init_resource_list(resource_list_t* list, const char* name, kresource_type_t type);
resource_list_t* create_resource_list(const char* name, kresource_type_t type);
void destroy_resource_list(resource_list_t* list);

ErrorOrPtr resource_add(resource_list_t* list, uintptr_t start, size_t size, kresource_flags_t flags);
ErrorOrPtr resource_add_ex(resource_list_t* list, kresource_t* prev, kresource_t* resource);

ErrorOrPtr resource_remove(resource_list_t* list, uintptr_t start, size_t size);
ErrorOrPtr resource_remove_ex(resource_list_t* list, kresource_t* resource);

ErrorOrPtr resource_get(resource_list_t* list, uintptr_t address);
ErrorOrPtr resource_put(resource_list_t* list, uintptr_t address);


#endif // !__ANIVA_RESOURCE_LIST__
