#include "core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/keyboard/ps2_keyboard.h"
#include "dev/manifest.h"
#include "libk/flow/error.h"
#include "libk/data/hive.h"
#include "libk/data/linkedlist.h"
#include "driver.h"
#include <mem/heap.h>
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/ipc/packet_payload.h"
#include "proc/ipc/packet_response.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include <entry/entry.h>

static hive_t* __installed_drivers;
static hive_t* __loaded_drivers;

static zone_allocator_t* __dev_manifest_allocator;

const char* dev_type_urls[DRIVER_TYPE_COUNT] = {
  [DT_DISK] = "disk",
  [DT_FS] = "fs",
  [DT_IO] = "io",
  [DT_SOUND] = "sound",
  [DT_GRAPHICS] = "graphics",
  [DT_OTHER] = "other",
  [DT_DIAGNOSTICS] = "diagnostics",
  [DT_SERVICE] = "service",
};

static list_t* __deferred_drivers;

#define DEV_MANIFEST_SOFTMAX 512

/*
 * We need to make sure to call this before we load a 
 * precompiled driver. Uninitialised fields might contain
 * non-null values
 */
static void asign_new_aniva_driver_manifest(aniva_driver_t* driver) {
  driver->m_manifest = 0;
}


static bool __load_precompiled_driver(aniva_driver_t* driver) {

  if (!driver)
    return false;

  asign_new_aniva_driver_manifest(driver);

  /* Trust that this will be loaded later */
  if (driver->m_flags & DRV_DEFERRED)
    return true;

  println(driver->m_name);
  load_driver(create_dev_manifest(driver, NULL));

  return true;
}


static bool walk_precompiled_drivers_to_load(hive_t* current, void* data) {
  return __load_precompiled_driver(data);
}


void init_aniva_driver_registry() {
  // TODO:
  __installed_drivers = create_hive("i_dev");
  __loaded_drivers = create_hive("l_dev");

  __dev_manifest_allocator = create_zone_allocator_ex(nullptr, NULL, DEV_MANIFEST_SOFTMAX * sizeof(dev_manifest_t), sizeof(dev_manifest_t), NULL);

  __deferred_drivers = init_list();

  // Install exported drivers
  FOREACH_PCDRV(ptr) {
    aniva_driver_t* driver = *ptr;

    ASSERT_MSG(driver, "Got an invalid precompiled driver! (ptr = NULL)");

    // NOTE: we should just let errors happen here,
    // since It could happen that a driver is already
    // loaded as a dependency. This means that this call
    // will always fail in that case, since we try to load
    // a driver that has already been loaded
    Must(install_driver(driver));
  }

  hive_walk(__installed_drivers, true, walk_precompiled_drivers_to_load);

  FOREACH(i, __deferred_drivers) {
    aniva_driver_t* driver = i->data;

    /* Skip invalid drivers, as a sanity check */
    if (!validate_driver(driver))
      continue;

    /* Clear the deferred flag */
    driver->m_flags &= ~DRV_DEFERRED;
  
    ASSERT_MSG(__load_precompiled_driver(driver), "Failed to load deferred precompiled driver!");
  }

  destroy_list(__deferred_drivers);
}


struct dev_manifest* allocate_dmanifest()
{
  if (!__dev_manifest_allocator)
    return nullptr;

  return zalloc_fixed(__dev_manifest_allocator);
}


void free_dmanifest(struct dev_manifest* manifest)
{
  if (!__dev_manifest_allocator)
    return;

  zfree_fixed(__dev_manifest_allocator, manifest);
}


static bool __should_defer(aniva_driver_t* driver)
{
  return ((driver->m_flags & DRV_SOCK) == DRV_SOCK);
}


ErrorOrPtr install_driver(aniva_driver_t* handle) {
  if (!validate_driver(handle)) {
    goto fail_and_exit;
  }

  if (is_driver_installed(handle)) {
    goto fail_and_exit;
  }

  dev_url_t path = get_driver_url(handle);

  ErrorOrPtr result = hive_add_entry(__installed_drivers, handle, path);

  if (result.m_status == ANIVA_FAIL) {
    goto fail_and_exit;
  }

  /* Mark this driver as deferred, so that we can delay its loading */
  if (__should_defer(handle)) {
    list_append(__deferred_drivers, handle);
    handle->m_flags |= DRV_DEFERRED;
  }

  return Success(0);

fail_and_exit:
  return Error();
}


/*
 * TODO: resolve dependencies!
 */
ErrorOrPtr uninstall_driver(aniva_driver_t* handle) {
  if (!handle)
    return Error();

  const char* driver_url = get_driver_url(handle);

  if (!validate_driver(handle)) {
    goto fail_and_exit;
  }

  if (!is_driver_installed(handle)) {
    goto fail_and_exit;
  }

  if (is_driver_loaded(handle)) {
    if (unload_driver(driver_url).m_status != ANIVA_SUCCESS) {
      goto fail_and_exit;
    }
  }

  if (hive_remove_path(__installed_drivers, driver_url).m_status != ANIVA_SUCCESS)
    goto fail_and_exit;

  // invalidate the manifest 
  kfree((void*)driver_url);
  return Success(0);

fail_and_exit:
  kfree((void*)driver_url);
  return Error();
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

  if (!manifest)
    return Error();

  aniva_driver_t* handle = manifest->m_handle;

  if (!validate_driver(handle)) {
    goto fail_and_exit;
  }

  // TODO: we can use ANIVA_FAIL_WITH_WARNING here, but we'll need to refactor some things
  // where we say result == ANIVA_FAIL
  // these cases will return false if we start using ANIVA_FAIL_WITH_WARNING here, so they will
  // need to be replaced with result != ANIVA_SUCCESS
  if (!is_driver_installed(manifest->m_handle)) {
    if (install_driver(manifest->m_handle).m_status != ANIVA_SUCCESS) {
      goto fail_and_exit;
    }
  }

  if (is_driver_loaded(handle)) {
    goto fail_and_exit;
  }

  FOREACH(i, manifest->m_dependency_manifests) {
    dev_manifest_t* dep_manifest = i->data;

    // TODO: check for errors
    if (dep_manifest && !is_driver_loaded(dep_manifest->m_handle)) {

      if (driver_is_deferred(dep_manifest->m_handle)) {
        print("Loading driver: ");
        println(handle->m_name);
        print("Loading dependency: ");
        println(dep_manifest->m_handle->m_name);
  
        kernel_panic("TODO: handle deferred dependencies!");
      }

      if (load_driver(dep_manifest).m_status != ANIVA_SUCCESS) {
        goto fail_and_exit;
      }
    }
  }

  dev_url_t path = manifest->m_url;

  ErrorOrPtr result = hive_add_entry(__loaded_drivers, handle, path);

  if (result.m_status != ANIVA_SUCCESS) {
    goto fail_and_exit;
  }

  dev_url_t full_path = hive_get_path(__loaded_drivers, handle);

  /* Link the driver to its manifest */
  handle->m_manifest = manifest;

  Must(bootstrap_driver(handle, full_path));

  return Success(0);

fail_and_exit:
  destroy_dev_manifest(manifest);
  return Error();
}


ErrorOrPtr unload_driver(dev_url_t url) {
  aniva_driver_t* handle = hive_get(__loaded_drivers, url);

  if (!validate_driver(handle) || strcmp(url, handle->m_manifest->m_url) != 0) {
    return Error();
  }

  // call the driver exit function async
  if (handle->m_flags & DRV_SOCK)
    driver_send_packet(handle->m_manifest->m_url, DCC_EXIT, NULL, 0);

  if (hive_remove(__loaded_drivers, handle).m_status != ANIVA_SUCCESS) {
    return Error();
  }

  // FIXME: if the manifest tells us the driver was loaded:
  //  - on the heap
  //  - through a file

  destroy_dev_manifest(handle->m_manifest);
  // TODO:
  return Success(0);
}


bool is_driver_loaded(struct aniva_driver* handle) {
  if (!__loaded_drivers) {
    return false;
  }
  return hive_contains(__loaded_drivers, handle);
}


bool is_driver_installed(struct aniva_driver* handle) {
  if (!__loaded_drivers || !__installed_drivers) {
    return false;
  }

  return hive_contains(__installed_drivers, handle);
}

/*
 * TODO: when checking this, it could be that the url
 * is totally invalid and results in a pagefault. In that 
 * case we want to be able to tell the pf handler that we 
 * can expect a pagefault and that when it happens, we should 
 * not terminate the kernel, but rather signal it to this
 * routine so that we can act accordingly
 */
dev_manifest_t* get_driver(dev_url_t url) {

  /* If we are asking for the current executing driver */
  if (url && strcmp(url, "this")) {

    /* 1) Check inside the current thread */
    thread_t* current_thread = get_current_scheduling_thread();

    aniva_driver_t* driver = thread_get_qdrv(current_thread);

    if (driver)
      return driver->m_manifest;

    /* 2) Check if we are initializing a driver at the moment (TODO) */
  }

  aniva_driver_t* handle = hive_get(__installed_drivers, url);

  if (handle == nullptr) {
    return nullptr;
  }

  // TODO: resolve dependencies and resources
  dev_manifest_t* manifest = create_dev_manifest(handle, NULL);

  // TODO: let the handle be nullable when creating a manifest
  if (manifest->m_handle != handle) {
    return nullptr;
  }

  return manifest;
}


dev_manifest_t* try_driver_get(aniva_driver_t* driver, uint32_t flags) {
  if (!driver || !driver->m_manifest) {
    return nullptr;
  }

  bool is_loaded = is_driver_loaded(driver);
  bool is_installed = is_driver_installed(driver);

  if ((driver->m_flags & DRV_ALLOW_DYNAMIC_LOADING) && (flags & DRV_ALLOW_DYNAMIC_LOADING)) {
    Must(load_driver(driver->m_manifest));
  }

  /* If the flags specify that the driver has to be active, we check for that */
  if ((driver->m_flags & DRV_ACTIVE) == 0 && (flags & DRV_ACTIVE)) {
    return nullptr;
  }

  if (is_loaded && is_installed) {
    return create_dev_manifest(driver, 0);
  }

  return nullptr;
}


/*
 * This function is kinda funky, since anyone who knows the url, can 
 * simply set the driver as ready for kicks and giggles. We might need
 * some security here lmao, so TODO/FIXME
 */
ErrorOrPtr driver_set_ready(const char* path) {

  // Fetch from the loaded drivers here, since unloaded
  // Drivers can never accept packets
  aniva_driver_t* handle = hive_get(__loaded_drivers, path);

  if (!handle)
    goto exit_invalid;

  handle->m_flags |= DRV_ACTIVE;

  // This driver is not a socket, and thus we can't find a thread
  // That is linked to this driver. We only give it a bootstrap thread
  // in this case
  if ((handle->m_flags & DRV_SOCK) == 0) {
    return Success(0);
  }

  /* This driver is mounted in the system fs. We could find it that way... */
  if (handle->m_flags & DRV_FS) {
    // TODO:
    kernel_panic("TODO: implement readying of FS drivers");
  }

  threaded_socket_t* socket = find_registered_socket(handle->m_port);

  if (!socket)
    goto exit_invalid;

  // When the socket is not marked as active, it certainly can't be
  // ready to recieve packets
  if (!socket_is_flag_set(socket, TS_ACTIVE))
    goto exit_invalid;

  socket_set_flag(socket, TS_READY, true);

  return Success(0);

exit_invalid:
  return Error();
}


ErrorOrPtr driver_send_packet(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size) {
  return driver_send_packet_a(path, code, buffer, buffer_size, nullptr, nullptr);
}


ErrorOrPtr driver_send_packet_a(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t* resp_buffer_size)
{
  aniva_driver_t* handle;

  if (!path)
    return Error();

  handle = hive_get(__loaded_drivers, path);

  return driver_send_packet_ex(handle, code, buffer, buffer_size, resp_buffer, resp_buffer_size);
}


/*
 * When resp_buffer AND resp_buffer_size are non-null, we assume the caller wants to
 * take in a direct response, in which case we can copy the response from the driver
 * directly into the buffer that the caller gave us
 */
ErrorOrPtr driver_send_packet_ex(struct aniva_driver* driver, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t* resp_buffer_size) 
{
  thread_t* current;

  if (!driver || !driver->f_drv_msg) 
    return Error();

  if ((driver->m_flags & DRV_SOCK) == 0) {

    packet_response_t* response;
    packet_payload_t* payload = create_packet_payload(buffer, buffer_size, code);

    uintptr_t result = driver->f_drv_msg(*payload, &response);

    if (response) {

      current = get_current_scheduling_thread();

      if (resp_buffer && resp_buffer_size) {

        /* Quick clamp to a valid size */
        if (*resp_buffer_size < response->m_response_size)
          *resp_buffer_size = response->m_response_size;

        memcpy(resp_buffer, response->m_response_buffer, *resp_buffer_size);

      /* If the sender (caller) thread is a socket, we can send our response there, otherwise its just lost to the void */
      } 
      else if (current && thread_is_socket(current)) {
        TRY(_, send_packet_to_socket_ex(current->m_socket->m_port, DCC_RESPONSE, response->m_response_buffer, response->m_response_size));
      }

      destroy_packet_response(response);
    }

    destroy_packet_payload(payload);

    return Success(0);
  }

  return send_packet_to_socket_ex(driver->m_port, code, buffer, buffer_size);
}


/*
 * NOTE: this function leaves behind a dirty packet response.
 * It is left to the caller to clean that up
 */
packet_response_t* driver_send_packet_sync(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size) {
  return driver_send_packet_sync_with_timeout(path, code, buffer, buffer_size, DRIVER_WAIT_UNTIL_READY);
}


#define LOCK_SOCKET_MAYBE(socket) do { if (socket) mutex_lock(socket->m_packet_mutex); } while(0)
#define UNLOCK_SOCKET_MAYBE(socket) do { if (socket) mutex_unlock(socket->m_packet_mutex); } while(0)

packet_response_t* driver_send_packet_sync_with_timeout(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, size_t mto) {
  aniva_driver_t* handle = hive_get(__loaded_drivers, path);

  if (!handle)
    goto exit_fail;

  threaded_socket_t* socket = nullptr;
  SocketOnPacket msg_func;

  if ((handle->m_flags & DRV_SOCK) == 0) {
    println("Driver is no socket");
    msg_func = handle->f_drv_msg;
  } else {
    socket = find_registered_socket(handle->m_port);
    msg_func = socket->m_on_packet;

    if (!socket)
      goto exit_fail;
  }
  //dev_manifest_t* manifest = create_dev_manifest(handle, 0);

  // TODO: validate checksums and funnie hashes
  // TODO: validate all dependencies are loaded

  // NOTE: this skips any verification by the packet multiplexer, and just 
  // kinda calls the drivers onPacket function whenever. This can result in
  // funny behaviour, so we should try to take the drivers packet mutex, in order 
  // to ensure we are the only ones asking this driver to handle some data

  if (!msg_func)
    goto exit_fail;

  size_t timeout = mto;

  /* NOTE: this is the same logic as that which is used in driver_is_ready(...) */
  while (!driver_is_ready(handle)) {

    scheduler_yield();

    if (timeout != DRIVER_WAIT_UNTIL_READY) {

      timeout--;
      if (timeout == 0) {
        goto exit_fail;
      }
    }
  }

  LOCK_SOCKET_MAYBE(socket);

  packet_response_t* response = nullptr;
  packet_payload_t* payload = create_packet_payload(buffer, buffer_size, code);

  msg_func(*payload, &response);

  // After the driver finishes what it is doing, we can just unlock the mutex.
  // We are done using the driver at this point after all
  UNLOCK_SOCKET_MAYBE(socket);

  destroy_packet_payload(payload);

  return response;

exit_fail:
  return nullptr;
}
