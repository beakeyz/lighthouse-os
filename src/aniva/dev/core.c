#include "core.h"
#include "dev/debug/test.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/manifest.h"
#include "libk/error.h"
#include "libk/hive.h"
#include "libk/linkedlist.h"
#include "driver.h"
#include <mem/heap.h>
#include "libk/string.h"
#include "sched/scheduler.h"

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

static hive_t* s_driver_hive;

void init_aniva_driver_registry() {
  // TODO:
  s_driver_hive = create_hive("dev");

}

void register_aniva_base_drivers() {

  pause_scheduler();
  // find all base drivers

  // load em 1 by 1

  dev_manifest_t* manifests_to_load[] = {
    create_dev_manifest((aniva_driver_t*)&g_base_ps2_keyboard_driver, nullptr, 0, "io.ps2", 0),
    create_dev_manifest((aniva_driver_t*)&g_test_dbg_driver, nullptr, 0, "diagnostics.debug", 0),
  };

  const size_t load_count = sizeof(manifests_to_load) / sizeof(dev_manifest_t*) ;

  for (uintptr_t i = 0; i < load_count; i++) {
    ErrorOrPtr result = load_driver(manifests_to_load[i]);

    if (result.m_status == ANIVA_FAIL) {
      kernel_panic("failed to load core driver");
    }
  }

  resume_scheduler();
}

/*
 * Steps to load a driver into our registry
 * 1: Resolve the url in the manifest
 * 2: Validate the driver using its manifest
 * 3: emplace the driver into the drivertree
 * 4: run driver bootstraps
 * 
 * We also might want to create a kernel-process for each driver
 * TODO: better security
 */
ErrorOrPtr load_driver(dev_manifest_t* manifest) {

  if (!validate_driver(manifest->m_handle)) {
    return Error();
  }

  if (is_driver_loaded(manifest->m_handle)) {
    return Error();
  }

  dev_url_t path = manifest->m_url;

  ErrorOrPtr result = hive_add_entry(s_driver_hive, manifest->m_handle, path);

  if (result.m_status == ANIVA_FAIL) {
    return result;
  }

  bootstrap_driver(manifest->m_handle);

  return Success(0);
}

ErrorOrPtr unload_driver(dev_manifest_t* manifest) {
  if (!validate_driver(manifest->m_handle)) {
    return Error();
  }

  // TODO:
  return Success(0);
}

bool is_driver_loaded(struct aniva_driver* handle) {
  // TODO:
  return false;
}

aniva_driver_t* get_driver(dev_url_t url) {
  aniva_driver_t* driver = hive_get(s_driver_hive, url);

  if (driver != nullptr) {
    return driver;
  }
  return nullptr;
}
