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

/* 
 * These hives give us more flexibility with finding drivers in nested paths,
 * since we enforce these type urls, but everything after that is free to choose
 */
static hive_t* __installed_driver_manifests;
static hive_t* __loaded_driver_manifests;

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

const char* driver_get_type_str(struct dev_manifest* driver)
{
  size_t type_str_len = NULL;
  dev_url_t drv_url = driver->m_url;

  while (*(drv_url + type_str_len) != DRIVER_URL_SEPERATOR) {
    type_str_len++;
  }

  for (uint64_t i = 0; i < DRIVER_TYPE_COUNT; i++) {
    if (memcmp(dev_type_urls[i], drv_url, type_str_len)) {
      return dev_type_urls[i];
    }
  }

  /* Invalid type part in the url */
  return nullptr;
}

dev_constraint_t __dev_constraints[DRIVER_TYPE_COUNT] = {
  [DT_DISK] = {
    .type = DT_DISK,
    .max_count = DRV_INFINITE,
    .current_count = 0,
  },
  [DT_FS] = {
    .type = DT_FS,
    .max_count = DRV_INFINITE,
    .current_count = 0,
  },
  [DT_IO] = {
    .type = DT_IO,
    .max_count = DRV_INFINITE,
    .current_count = 0,
  },
  [DT_SOUND] = {
    .type = DT_SOUND,
    .max_count = 1,
    .current_count = 0,
  },
  [DT_GRAPHICS] = {
    .type = DT_GRAPHICS,
    .max_count = 1,
    .current_count = 0,
  },
  [DT_OTHER] = {
    .type = DT_OTHER,
    .max_count = DRV_INFINITE,
    .current_count = 0,
  },
  [DT_DIAGNOSTICS] = {
    .type = DT_DIAGNOSTICS,
    .max_count = 1,
    .current_count = 0,
  },
  [DT_SERVICE] = {
    .type = DT_SERVICE,
    .max_count = DRV_SERVICE_MAX,
    .current_count = 0,
  },
};

static list_t* __deferred_driver_manifests;

static bool __load_precompiled_driver(dev_manifest_t* manifest) {

  if (!manifest)
    return false;

  /* Trust that this will be loaded later */
  if (manifest->m_flags & DRV_DEFERRED)
    return true;

  manifest_gather_dependencies(manifest);

  /*
   * NOTE: this fails if the driver is already loaded, but we ignore this
   * since we also load drivers preemptively if they are needed as a dependency
   */
  load_driver(manifest);

  return true;
}


static bool walk_precompiled_drivers_to_load(hive_t* current, void* data) {
  return __load_precompiled_driver(data);
}


void init_aniva_driver_registry() {
  __installed_driver_manifests = create_hive("i_dev");
  __loaded_driver_manifests = create_hive("l_dev");

  __dev_manifest_allocator = create_zone_allocator_ex(nullptr, NULL, DEV_MANIFEST_SOFTMAX * sizeof(dev_manifest_t), sizeof(dev_manifest_t), NULL);

  __deferred_driver_manifests = init_list();

  println("Loading things");
  // Install exported drivers
  FOREACH_PCDRV(ptr) {

    dev_manifest_t* manifest;
    aniva_driver_t* driver = *ptr;

    println("Loading: ");
    println(driver->m_name);
    println(to_string((uintptr_t)driver));
    ASSERT_MSG(driver, "Got an invalid precompiled driver! (ptr = NULL)");

    manifest = create_dev_manifest(driver);

    ASSERT_MSG(manifest, "Failed to create manifest for a precompiled driver!");

    // NOTE: we should just let errors happen here,
    // since It could happen that a driver is already
    // loaded as a dependency. This means that this call
    // will always fail in that case, since we try to load
    // a driver that has already been loaded
    Must(install_driver(manifest));
  }

  hive_walk(__installed_driver_manifests, true, walk_precompiled_drivers_to_load);

  FOREACH(i, __deferred_driver_manifests) {
    dev_manifest_t* manifest = i->data;

    /* Skip invalid drivers, as a sanity check */
    if (!verify_driver(manifest->m_handle))
      continue;

    /* Clear the deferred flag */
    manifest->m_flags &= ~DRV_DEFERRED;
  
    ASSERT_MSG(__load_precompiled_driver(manifest), "Failed to load deferred precompiled driver!");
  }

  destroy_list(__deferred_driver_manifests);
  __deferred_driver_manifests = nullptr;
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


bool verify_driver(struct aniva_driver* driver) 
{
  dev_constraint_t* constaint;

  if (!driver || !driver->f_init) {
    return false;
  }

  if (driver->m_type >= DRIVER_TYPE_COUNT)
    return false;

  constaint = &__dev_constraints[driver->m_type];

  /* We can't load more of this type of driver, so we mark this as invalid */
  if (constaint->current_count == constaint->max_count) {
    return false;
  }

  return true;
}

static bool __should_defer(dev_manifest_t* driver)
{
  return ((driver->m_flags & DRV_SOCK) == DRV_SOCK);
}


ErrorOrPtr install_driver(dev_manifest_t* manifest) {

  ErrorOrPtr result;

  if (!verify_driver(manifest->m_handle)) {
    goto fail_and_exit;
  }

  if (is_driver_installed(manifest)) {
    goto fail_and_exit;
  }

  result = hive_add_entry(__installed_driver_manifests, manifest, manifest->m_url);

  if (IsError(result)) {
    goto fail_and_exit;
  }

  /* Mark this driver as deferred, so that we can delay its loading */
  if (__deferred_driver_manifests && __should_defer(manifest)) {
    list_append(__deferred_driver_manifests, manifest);
    manifest->m_flags |= DRV_DEFERRED;
  }

  return Success(0);

fail_and_exit:
  if (manifest)
    destroy_dev_manifest(manifest);
  return Error();
}


/*
 * TODO: resolve dependencies!
 */
ErrorOrPtr uninstall_driver(dev_manifest_t* manifest) {
  if (!manifest)
    return Error();

  if (!verify_driver(manifest->m_handle)) {
    goto fail_and_exit;
  }

  if (!is_driver_installed(manifest)) {
    goto fail_and_exit;
  }

  if (is_driver_loaded(manifest)) {
    if (unload_driver(manifest->m_url).m_status != ANIVA_SUCCESS) {
      goto fail_and_exit;
    }
  }

  if (hive_remove_path(__installed_driver_manifests, manifest->m_url).m_status != ANIVA_SUCCESS)
    goto fail_and_exit;

  // invalidate the manifest 
  //kfree((void*)driver_url);
  return Success(0);

fail_and_exit:
  //kfree((void*)driver_url);
  return Error();
}


static void __driver_register_presence(DEV_TYPE type) 
{
  if (type >= DRIVER_TYPE_COUNT)
    return;

  __dev_constraints[type].current_count++;
}

static void __driver_unregister_presence(DEV_TYPE type) 
{
  if (type >= DRIVER_TYPE_COUNT || __dev_constraints[type].current_count)
    return;

  __dev_constraints[type].current_count--;
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

  ErrorOrPtr result;
  aniva_driver_t* handle;

  if (!manifest)
    return Error();

  handle = manifest->m_handle;

  if (!verify_driver(handle))
    goto fail_and_exit;

  // TODO: we can use ANIVA_FAIL_WITH_WARNING here, but we'll need to refactor some things
  // where we say result == ANIVA_FAIL
  // these cases will return false if we start using ANIVA_FAIL_WITH_WARNING here, so they will
  // need to be replaced with result != ANIVA_SUCCESS
  if (!is_driver_installed(manifest)) {
    if (IsError(install_driver(manifest))) {
      goto fail_and_exit;
    }
  }

  if (is_driver_loaded(manifest))
    goto fail_and_exit;

  FOREACH(i, manifest->m_dependency_manifests) {
    dev_manifest_t* dep_manifest = i->data;

    // TODO: check for errors
    if (dep_manifest && !is_driver_loaded(dep_manifest)) {

      if (driver_is_deferred(dep_manifest)) {
        print("Loading driver: ");
        println(handle->m_name);
        print("Loading dependency: ");
        println(dep_manifest->m_handle->m_name);
  
        kernel_panic("TODO: handle deferred dependencies!");
      }

      if (IsError(load_driver(dep_manifest))) {
        goto fail_and_exit;
      }
    }
  }

  result = hive_add_entry(__loaded_driver_manifests, manifest, manifest->m_url);

  if (IsError(result)) {
    goto fail_and_exit;
  }

  __driver_register_presence(handle->m_type);

  /*
   * TODO/NOTE: We wrap this bootstrap in a Must(), since we still need 
   * sufficient handling for drivers that try to pull oopsies
   * (Like checking if we ACTUALLY need the driver)
   */
  Must(bootstrap_driver(manifest));

  return Success(0);

fail_and_exit:
  destroy_dev_manifest(manifest);
  return Error();
}


ErrorOrPtr unload_driver(dev_url_t url) {
  dev_manifest_t* manifest = hive_get(__loaded_driver_manifests, url);

  if (!verify_driver(manifest->m_handle) || strcmp(url, manifest->m_url) != 0) {
    return Error();
  }

  // call the driver exit function async
  if (manifest->m_flags & DRV_SOCK)
    driver_send_packet(manifest->m_url, DCC_EXIT, NULL, 0);

  if (hive_remove(__loaded_driver_manifests, manifest).m_status != ANIVA_SUCCESS) {
    return Error();
  }

  __driver_unregister_presence(manifest->m_handle->m_type);

  //destroy_dev_manifest(manifest);
  // TODO:
  return Success(0);
}


bool is_driver_loaded(dev_manifest_t* manifest) {
  if (!__loaded_driver_manifests) {
    return false;
  }

  return hive_contains(__loaded_driver_manifests, manifest);
}


bool is_driver_installed(dev_manifest_t* manifest) {
  if (!__loaded_driver_manifests || !__installed_driver_manifests) {
    return false;
  }

  return hive_contains(__installed_driver_manifests, manifest);
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
  if (url && strcmp(url, "this") == 0) {
    kernel_panic("TODO: implement get_driver(\"this\")");
  }

   return hive_get(__installed_driver_manifests, url);
}


dev_manifest_t* try_driver_get(aniva_driver_t* driver, uint32_t flags) {

  dev_url_t path;
  dev_manifest_t* ret;

  if (!driver) {
    return nullptr;
  }

  path = get_driver_url(driver);

  ret = get_driver(path);

  if (!ret)
    goto fail_and_exit;

  if (!is_driver_loaded(ret))
    goto fail_and_exit;

  /* If the flags specify that the driver has to be active, we check for that */
  if ((ret->m_flags & DRV_ACTIVE) == 0 && (flags & DRV_ACTIVE))
    goto fail_and_exit;

  return ret;

fail_and_exit:
  kfree((void*)path);
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
  dev_manifest_t* manifest = hive_get(__loaded_driver_manifests, path);

  if (!manifest)
    goto exit_invalid;

  manifest->m_flags |= DRV_ACTIVE;

  // This driver is not a socket, and thus we can't find a thread
  // That is linked to this driver. We only give it a bootstrap thread
  // in this case
  if ((manifest->m_flags & DRV_SOCK) == 0) {
    return Success(0);
  }

  /* This driver is mounted in the system fs. We could find it that way... */
  if (manifest->m_flags & DRV_FS) {
    // TODO:
    kernel_panic("TODO: implement readying of FS drivers");
  }

  threaded_socket_t* socket = find_registered_socket(manifest->m_handle->m_port);

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
  dev_manifest_t* manifest;

  if (!path)
    return Error();

  manifest = hive_get(__loaded_driver_manifests, path);

  return driver_send_packet_ex(manifest, code, buffer, buffer_size, resp_buffer, resp_buffer_size);
}


/*
 * When resp_buffer AND resp_buffer_size are non-null, we assume the caller wants to
 * take in a direct response, in which case we can copy the response from the driver
 * directly into the buffer that the caller gave us
 */
ErrorOrPtr driver_send_packet_ex(struct dev_manifest* manifest, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t* resp_buffer_size) 
{
  uintptr_t error;
  thread_t* current;
  aniva_driver_t* driver;

  if (!manifest || (manifest->m_flags & DMAN_FLAG_HAS_MSG_FUNC) == 0)
    return Error();

  driver = manifest->m_handle;

  if (!driver->f_drv_msg) 
    return Error();

  if ((manifest->m_flags & DRV_SOCK) == 0) {

    packet_response_t* response = nullptr;
    packet_payload_t payload;

    init_packet_payload(&payload, nullptr, buffer, buffer_size, code);

    error = driver->f_drv_msg(payload, &response);

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

    destroy_packet_payload(&payload);

    if (error)
      return Error();

    return Success(0);
  }

  /* DEPRECATED: i've decided that socket drivers are fucking stupid lol */
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
  dev_manifest_t* manifest = hive_get(__loaded_driver_manifests, path);

  if (!manifest)
    goto exit_fail;

  threaded_socket_t* socket = nullptr;
  SocketOnPacket msg_func;

  if ((manifest->m_flags & DRV_SOCK) == 0) {
    msg_func = manifest->m_handle->f_drv_msg;
  } else {
    socket = find_registered_socket(manifest->m_handle->m_port);
    msg_func = socket->f_on_packet;

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
  while (!driver_is_ready(manifest)) {

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
  packet_payload_t payload;

  init_packet_payload(&payload, nullptr, buffer, buffer_size, code);

  msg_func(payload, &response);

  // After the driver finishes what it is doing, we can just unlock the mutex.
  // We are done using the driver at this point after all
  UNLOCK_SOCKET_MAYBE(socket);

  destroy_packet_payload(&payload);

  return response;

exit_fail:
  return nullptr;
}
