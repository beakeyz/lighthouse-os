#include "external.h"
#include "mem/zalloc.h"
#include "system/resource.h"

extern_driver_t* create_external_driver(uint32_t flags)
{
  extern_driver_t* drv;

  drv = kzalloc(sizeof(extern_driver_t));

  drv->m_process = NULL;
  drv->m_file = NULL;
  drv->m_flags = flags;
  drv->m_manifest = NULL; 

  create_resource_bundle(&drv->m_resources);

  return drv;
}

/*
 * TODO: move this resource clear routine to the resource context subsystem
 */
static void __ext_driver_clear_resources(extern_driver_t* driver)
{
  ErrorOrPtr result;
  kresource_t* start_resource;
  kresource_t* current;
  kresource_t* next;

  if (!*driver->m_resources)
    return;

  for (kresource_type_t type = 0; type < KRES_MAX_TYPE; type++) {

    /* Find the first resource of this type */
    current = start_resource = driver->m_resources[type];

    while (current) {

      next = current->m_next;

      uintptr_t start = current->m_start;
      size_t size = current->m_size;

      /* Skip mirrors with size zero or no references */
      if (!size || !current->m_shared_count) {
        goto next_resource;
      }

      /* TODO: destry other resource types */
      switch (current->m_type) {
        case KRES_TYPE_MEM:

          /* Should we dealloc or simply unmap? */
          if ((current->m_flags & KRES_FLAG_MEM_KEEP_PHYS) == KRES_FLAG_MEM_KEEP_PHYS) {

            /* Preset */
            result = Error();

            /* Try to unmap */
            if (kmem_unmap_range_ex(nullptr, start, GET_PAGECOUNT(size), KMEM_CUSTOMFLAG_RECURSIVE_UNMAP)) {

              /* Pre-emptively remove the flags, just in case this fails */
              current->m_flags &= ~KRES_FLAG_MEM_KEEP_PHYS;

              /* Yay, now release the thing */
              result = resource_release(start, size, start_resource);
            }

            break;
          }

          result = __kmem_dealloc_ex(nullptr, driver->m_resources, start, size, false, true);
          break;
        default:
          /* Skip this entry for now */
          break;
      }

      /* If we successfully dealloc, the resource, we cant trust the link anymore. Start over */
      if (!IsError(result))
        goto reset;

next_resource:
      if (current->m_next) {
        ASSERT_MSG(current->m_type == current->m_next->m_type, "Found type mismatch while cleaning process resources");
      }

      current = next;
      continue;

reset:
      current = start_resource;
    }
  }

  /* Destroy the entire bundle, which deallocates the structures */
  destroy_resource_bundle(driver->m_resources);
}

void destroy_external_driver(extern_driver_t* driver)
{
  __ext_driver_clear_resources(driver);
  kzfree(driver, sizeof(extern_driver_t));
}

