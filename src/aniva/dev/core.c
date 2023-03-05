#include "core.h"
#include "dev/manifest.h"
#include "libk/linkedlist.h"
#include "driver.h"
#include <mem/heap.h>
#include "libk/string.h"

#define DRIVER_TYPE_COUNT 5

const static DEV_TYPE_t dev_types[] = {
  DT_DISK,
  DT_IO,
  DT_SOUND,
  DT_GRAPHICS,
  DT_OTHER,
  DT_DIAGNOSTICS,
  DT_SERVICE,
};

void init_aniva_driver_registry() {
  // TODO:

}

/*
 * Steps to load a driver into our registry
 * 1: Resolve the url in the manifest
 * 2: Validate the driver using its manifest
 * 3: emplace the driver into the drivertree
 * 4: run driver bootstraps
 * 
 * We also might want to create a kernel-process for each driver
 */
void load_driver(dev_manifest_t* manifest) {

  if (!validate_driver(manifest->m_handle)) {
    return;
  }

  // TODO:
}

void unload_driver(dev_manifest_t* manifest) {
  if (!validate_driver(manifest->m_handle)) {
    return;
  }

  // TODO:
}
