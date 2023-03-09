#include "core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/manifest.h"
#include "libk/error.h"
#include "libk/hive.h"
#include "libk/linkedlist.h"
#include "driver.h"
#include <mem/heap.h>
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "proc/core.h"
#include "sched/scheduler.h"



static hive_t* s_installed_drivers;
static hive_t* s_loaded_drivers;

void init_aniva_driver_registry() {
  // TODO:
  s_installed_drivers = create_hive("i_dev");
  s_loaded_drivers = create_hive("l_dev");

}

void register_aniva_base_drivers() {

  pause_scheduler();
  // find all base drivers

  // load em 1 by 1

  dev_manifest_t* manifests_to_load[] = {
    create_dev_manifest((aniva_driver_t*)&g_base_ps2_keyboard_driver, nullptr, 0, 0),
    create_dev_manifest((aniva_driver_t*)&g_test_dbg_driver, nullptr, 0, 0),
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

ErrorOrPtr install_driver(struct dev_manifest* manifest) {
  if (!validate_driver(manifest->m_handle)) {
    return Error();
  }

  if (is_driver_installed(manifest->m_handle)) {
    return Error();
  }

  dev_url_t path = manifest->m_url;

  ErrorOrPtr result = hive_add_entry(s_installed_drivers, manifest->m_handle, path);

  if (result.m_status == ANIVA_FAIL) {
    return result;
  }
  return Success(0);
}

ErrorOrPtr uninstall_driver(struct dev_manifest* manifest) {
  if (!validate_driver(manifest->m_handle)) {
    return Error();
  }

  if (!is_driver_installed(manifest->m_handle)) {
    return Error();
  }

  if (is_driver_loaded(manifest->m_handle)) {
    TRY(unload_driver(manifest->m_url));
  }

  dev_url_t path = manifest->m_url;

  TRY(hive_remove_path(s_installed_drivers, path));

  // invalidate the manifest 
  destroy_dev_manifest(manifest);
  return Success(0);

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

  if (!is_driver_installed(manifest->m_handle)) {
    ErrorOrPtr install_result = install_driver(manifest);
    if (install_result.m_status == ANIVA_FAIL) {
      return install_result;
    }
    //return Error();
  }

  if (is_driver_loaded(manifest->m_handle)) {
    return Error();
  }

  dev_url_t path = manifest->m_url;

  ErrorOrPtr result = hive_add_entry(s_loaded_drivers, manifest->m_handle, path);

  if (result.m_status == ANIVA_FAIL) {
    return result;
  }

  bootstrap_driver(manifest->m_handle);

  return Success(0);
}

ErrorOrPtr unload_driver(dev_url_t url) {
  dev_manifest_t* dummy_manifest = create_dev_manifest(hive_get(s_loaded_drivers, url), nullptr, 0, 0);

  if (!validate_driver(dummy_manifest->m_handle) || strcmp(url, dummy_manifest->m_url) != 0) {
    return Error();
  }

  // call the driver exit function async
  driver_send_packet(dummy_manifest->m_url, DCC_EXIT, NULL, 0);

  TRY(hive_remove(s_loaded_drivers, dummy_manifest->m_handle));

  // FIXME: if the manifest tells us the driver was loaded:
  //  - on the heap
  //  - through a file

  destroy_dev_manifest(dummy_manifest);
  // TODO:
  return Success(0);
}

bool is_driver_loaded(struct aniva_driver* handle) {
  return hive_contains(s_loaded_drivers, handle);
}

bool is_driver_installed(struct aniva_driver* handle) {
  return hive_contains(s_installed_drivers, handle);
}

dev_manifest_t* get_driver(dev_url_t url) {
  aniva_driver_t* handle = hive_get(s_installed_drivers, url);

  if (handle == nullptr) {
    return nullptr;
  }

  // TODO: resolve dependencies and resources
  dev_manifest_t* manifest = create_dev_manifest(handle, NULL, 0, 0);

  // TODO: let the handle be nullable when creating a manifest
  if (manifest->m_handle != handle) {
    return nullptr;
  }

  return manifest;
}

async_ptr_t* driver_send_packet(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size) {

  aniva_driver_t* handle = hive_get(s_loaded_drivers, path);

  if (!handle)
    return nullptr;

  return send_packet_to_socket_with_code(handle->m_port, code, buffer, buffer_size);
}
