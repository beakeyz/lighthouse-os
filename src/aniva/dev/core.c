#include "core.h"
#include "dev/debug/serial.h"
#include "dev/debug/test.h"
#include "dev/external.h"
#include "dev/loader.h"
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
#include "proc/proc.h"
#include "proc/socket.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/asm_specifics.h"
#include "system/processor/processor.h"
#include <entry/entry.h>

/* 
 * These hives give us more flexibility with finding drivers in nested paths,
 * since we enforce these type urls, but everything after that is free to choose
 */
static hive_t* __installed_driver_manifests = nullptr;
//static hive_t* __loaded_driver_manifests = nullptr;

static zone_allocator_t* __dev_manifest_allocator;

/* This lock protects the core driver registry */
static mutex_t* __core_driver_lock;

const char* dev_type_urls[DRIVER_TYPE_COUNT] = {
  /* Any driver that interacts with lts */
  [DT_DISK] = "disk",
  /* Drivers that interact with or implement filesystems */
  [DT_FS] = "fs",
  /* Drivers that deal with I/O devices (HID, serial, ect.) */
  [DT_IO] = "io",
  /* Drivers that manage sound devices */
  [DT_SOUND] = "sound",
  /* Drivers that manage graphics devices */
  [DT_GRAPHICS] = "graphics",
  /* Drivers that don't really deal with specific types of devices */
  [DT_OTHER] = "other",
  /* Drivers that gather system diagnostics */
  [DT_DIAGNOSTICS] = "diagnostics",
  /* Drivers that provide services, either to userspace or kernelspace */
  [DT_SERVICE] = "service",
  /* Drivers that implement a core platform for other drivers to function */
  [DT_CORE] = "core",
};

/*
 * Core drivers
 *
 * Let's create a quick overview of what core drivers are supposed to do for us. As an example, Let's 
 * imagine ourselves a system that booted from UEFI and got handed a GOP framebuffer from our bootloader.
 * When this happens, we are all well and happy and we load the efifb driver like a good little kernel. However,
 * this system also has dedicated graphics hardware and we also want smooth and clean graphics pretty please. For this,
 * we'll implement a core driver that sits on top of any graphics driver and manages their states. When we try to probe for 
 * graphics hardware and we find something we can support with a driver, we'll try to load the driver and it will register itself 
 * at the core graphics driver (core/video). The core driver will then determine if we want to keep the old driver, or replace 
 * it with this new and more fancy driver (through the use of precedence). 
 * When it comes to external applications that want to make use of any graphics interface (lwnd, most likely), we also want
 * it to have an easy time setting up a graphics environment. Any applications that make use of graphics should not have to figure
 * out which graphics driver is active on the system, but should make their requests through a middleman (core/video) that is aware
 * of the underlying device structure
 */

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

static mutex_t* __driver_constraint_lock;

static dev_constraint_t __dev_constraints[DRIVER_TYPE_COUNT] = {
  [DT_DISK] = {
    .type = DT_DISK,
    .max_count = DRV_INFINITE,
    .max_active = DRV_INFINITE,
    0
  },
  [DT_FS] = {
    .type = DT_FS,
    .max_count = DRV_INFINITE,
    .max_active = DRV_INFINITE,
    0
  },
  [DT_IO] = {
    .type = DT_IO,
    .max_count = DRV_INFINITE,
    .max_active = DRV_INFINITE,
    0
  },
  [DT_SOUND] = {
    .type = DT_SOUND,
    .max_count = 10,
    .max_active = 1,
    0
  },
  /*
   * Most graphics drivers should be external 
   * This means that When we are booting the kernel we load a local driver for the 
   * efi framebuffer and then we detect available graphics cards later. When we find
   * one we have a driver for, we find it in the fs and load it. When loading the graphics 
   * driver (which is most likely on PCI) we will keep the EFI fb driver intact untill the 
   * driver reports successful load. We can do this because the external driver scan will happen AFTER we have 
   * scanned PCI for local drivers, so when we load the external graphics driver, the
   * load will also probe PCI, which will load the full driver in one go. When this reports
   * success, we can unload the EFI driver and switch the core to the new external driver
   */
  [DT_GRAPHICS] = {
    .type = DT_GRAPHICS,
    .max_count = 10,
    .max_active = 2,
    0
  },
  [DT_OTHER] = {
    .type = DT_OTHER,
    .max_count = DRV_INFINITE,
    .max_active = DRV_INFINITE,
    0
  },
  [DT_DIAGNOSTICS] = {
    .type = DT_DIAGNOSTICS,
    .max_count = 1,
    .max_active = 1,
    0
  },
  [DT_SERVICE] = {
    .type = DT_SERVICE,
    .max_count = DRV_SERVICE_MAX,
    .max_active = DRV_SERVICE_MAX,
    0
  },
  [DT_CORE] = {
    .type = DT_CORE,
    .max_count = DRV_INFINITE,
    .max_active = DRV_INFINITE,
    0
  },
};

static list_t* __deferred_driver_manifests;

static bool __load_precompiled_driver(dev_manifest_t* manifest) {

  if (!manifest)
    return false;

  /* Trust that this will be loaded later */
  if (manifest->m_flags & DRV_DEFERRED)
    return true;

  /* Skip already loaded drivers */
  if ((manifest->m_flags & DRV_LOADED) == DRV_LOADED)
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
  __installed_driver_manifests = create_hive("Devices");
  __dev_manifest_allocator = create_zone_allocator_ex(nullptr, NULL, DEV_MANIFEST_SOFTMAX * sizeof(dev_manifest_t), sizeof(dev_manifest_t), NULL);
  __deferred_driver_manifests = init_list();

  __driver_constraint_lock = create_mutex(NULL);
  __core_driver_lock = create_mutex(NULL);

  FOREACH_CORE_DRV(ptr) {

    dev_manifest_t* manifest;
    aniva_driver_t* driver = *ptr;

    ASSERT_MSG(driver, "Got an invalid precompiled driver! (ptr = NULL)");

    manifest = create_dev_manifest(driver);

    ASSERT_MSG(manifest, "Failed to create manifest for a precompiled driver!");

    Must(install_driver(manifest));
  }

  /* First load pass */
  hive_walk(__installed_driver_manifests, true, walk_precompiled_drivers_to_load);

  // Install exported drivers
  FOREACH_PCDRV(ptr) {

    dev_manifest_t* manifest;
    aniva_driver_t* driver = *ptr;

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

  /* Second load pass, with the core drivers already loaded */
  hive_walk(__installed_driver_manifests, true, walk_precompiled_drivers_to_load);

  FOREACH(i, __deferred_driver_manifests) {
    dev_manifest_t* manifest = i->data;

    /* Skip invalid drivers, as a sanity check */
    if (!verify_driver(manifest))
      continue;

    /* Clear the deferred flag */
    manifest->m_flags &= ~DRV_DEFERRED;
  
    ASSERT_MSG(__load_precompiled_driver(manifest), "Failed to load deferred precompiled driver!");
  }

  destroy_list(__deferred_driver_manifests);
  __deferred_driver_manifests = nullptr;
}


dev_manifest_t* allocate_dmanifest()
{
  dev_manifest_t* ret;

  if (!__dev_manifest_allocator)
    return nullptr;

  ret = zalloc_fixed(__dev_manifest_allocator);

  memset(ret, 0, sizeof(dev_manifest_t));

  return ret;
}


void free_dmanifest(struct dev_manifest* manifest)
{
  if (!__dev_manifest_allocator)
    return;

  zfree_fixed(__dev_manifest_allocator, manifest);
}

/*!
 * @brief Verify the validity of a driver and also if it is clear to be loaded
 *
 * @returns: false on failure (the driver is invalid or not clear to be loaded) 
 * true on success (driver is valid)
 */
bool verify_driver(dev_manifest_t* manifest) 
{
  dev_constraint_t* constaint;
  aniva_driver_t* driver;

  driver = manifest->m_handle;

  if (!driver || !driver->f_init) {
    return false;
  }

  if (driver->m_type >= DRIVER_TYPE_COUNT)
    return false;

  constaint = &__dev_constraints[driver->m_type];

  /* We can't load more of this type of driver, so we mark this as invalid */
  if (constaint->current_count == constaint->max_count) {
    /* If our new driver has a higher precedence, we can continue loading */
    if (manifest->m_handle->m_precedence <= constaint->active->m_handle->m_precedence)
      return false;
  }

  return true;
}

static bool __should_defer(dev_manifest_t* driver)
{
  /* TODO: */
  (void)driver;
  return false;
}


ErrorOrPtr install_driver(dev_manifest_t* manifest) 
{
  ErrorOrPtr result;

  if (!manifest)
    goto fail_and_exit;

  if (!verify_driver(manifest))
    goto fail_and_exit;

  if (is_driver_installed(manifest))
    goto fail_and_exit;

  result = hive_add_entry(__installed_driver_manifests, manifest, manifest->m_url);

  if (IsError(result))
    goto fail_and_exit;

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


/*!
 * @brief Detaches the manifests path from the driver tree and destroys the manifest
 *
 * Nothing to add here...
 * TODO: resolve dependencies!
 */
ErrorOrPtr uninstall_driver(dev_manifest_t* manifest) 
{
  if (!manifest)
    return Error();

  /* Uninstalled stuff can't be uninstalled */
  if (!is_driver_installed(manifest))
    goto fail_and_exit;

  /* When we fail to unload something, thats quite bad lmao */
  if (is_driver_loaded(manifest) && IsError(unload_driver(manifest->m_url)))
    goto fail_and_exit;

  /* Remove the manifest from the driver tree */
  if (IsError(hive_remove_path(__installed_driver_manifests, manifest->m_url)))
    goto fail_and_exit;

  destroy_dev_manifest(manifest);

  return Success(0);

fail_and_exit:
  //kfree((void*)driver_url);
  return Error();
}

/*
static void __driver_register_active(dev_type_t type, dev_manifest_t* manifest)
{
  dev_manifest_t* c_active_manifest;
  dev_constraint_t* constraint = &__dev_constraints[type];

  c_active_manifest = constraint->active;

  // Easy job yay
  if (constraint->max_active == 1) {
    switch (constraint->current_active) {
      case 0:
        {
          constraint->active = manifest;
          constraint->current_active++;
          break;
        }
      case 1:
        {
          ASSERT_MSG(c_active_manifest && c_active_manifest->m_handle, "No active manifest, wtf!");

          // In this case, the driver has already registered itself as the active driver
          if (c_active_manifest == manifest)
            break;

          // If there is a reason to switch, we do
          if (manifest->m_handle->m_precedence > c_active_manifest->m_handle->m_precedence) {

            // Unload the current active driver
            unload_driver(c_active_manifest->m_url);

            constraint->active = manifest;
          }
          break;
        }
      default:
        break;
    }
    return;
  }

  ASSERT_MSG(!c_active_manifest, "active manifest isn't null while we are allowing more than one active drivers!");

  ASSERT_MSG(constraint->current_active < constraint->max_active, "Exceeded maximum amount of active drivers for this type. TODO: implement error handling here");

  constraint->current_active++;
}
*/

static void __driver_unregister_active(dev_type_t type, dev_manifest_t* manifest)
{
  dev_constraint_t* constraint = &__dev_constraints[type];

  /* We are not dealing with the current active driver */
  if (constraint->active != manifest || !constraint->current_active)
    return;

  mutex_lock(__driver_constraint_lock);

  /* Just make sure this is null if we unregister active */
  constraint->active = nullptr;

  constraint->current_active--;

  mutex_unlock(__driver_constraint_lock);
}

static void __driver_register_presence(dev_type_t type) 
{
  if (type >= DRIVER_TYPE_COUNT)
    return;

  mutex_lock(__driver_constraint_lock);
  __dev_constraints[type].current_count++;
  mutex_unlock(__driver_constraint_lock);
}

static void __driver_unregister_presence(dev_type_t type) 
{
  if (type >= DRIVER_TYPE_COUNT || __dev_constraints[type].current_count)
    return;

  mutex_lock(__driver_constraint_lock);
  __dev_constraints[type].current_count--;
  mutex_unlock(__driver_constraint_lock);
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
  extern_driver_t* ext_driver;
  aniva_driver_t* handle;

  if (!manifest)
    return Error();

  if (!verify_driver(manifest))
    goto fail_and_exit;

  ext_driver = nullptr;

  /* This driver was unloaded before. Check if we can do some cool shit */
  if ((manifest->m_flags & DRV_WAS_UNLOADED) == DRV_WAS_UNLOADED) {

    /* Clear this flag, since it might fuck us if we don't */
    manifest->m_flags &= DRV_WAS_UNLOADED;

    if ((manifest->m_flags & DRV_IS_EXTERNAL) == DRV_IS_EXTERNAL && manifest->m_driver_file_path)
      ext_driver = load_external_driver_manifest(manifest);

    /* If we fail to load the driver from cache, just go through the entire process again... */
    if (ext_driver)
      return Success(0);

    /* Make sure to clean up our manifset */
    if (manifest->m_driver_file_path)
      kfree((void*)manifest->m_driver_file_path);

    manifest->m_driver_file_path = nullptr;
    manifest->m_external = nullptr;
    manifest->m_handle = nullptr;
    manifest->m_flags &= ~(DRV_IS_EXTERNAL | DRV_LOADED | DRV_HAS_HANDLE);
  }

  handle = manifest->m_handle;

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
      print("Loading driver: ");
      println(handle->m_name);
      print("Loading dependency: ");
      println(dep_manifest->m_handle->m_name);

      if (driver_is_deferred(dep_manifest)) {
        kernel_panic("TODO: handle deferred dependencies!");
      }

      if (IsError(load_driver(dep_manifest))) {
        goto fail_and_exit;
      }
    }
  }

  result = bootstrap_driver(manifest);

  /* If the manifest says something went wrong, trust that */
  if (IsError(result) || (manifest->m_flags & DRV_FAILED)) {
    unload_driver(manifest->m_url);
    return Error();
  }

  __driver_register_presence(handle->m_type);

  /*
   * TODO: we need to detect when we want to apply the 'activity' and 'precedence' mechanisms
   * since we always load a bunch of graphics drivers at startup. We need to select one once all the devices should be set up
   * and then remove all the unused drivers once they are gracefully exited
   *
   * Maybe we simply do what linux drm does and have drivers mark themselves as the 'active' driver by removing any lingering
   * apperature...
   * 
   */
  //__driver_register_active(handle->m_type, manifest);

  manifest->m_flags |= DRV_LOADED;

  return Success(0);

fail_and_exit:
  return Error();
}


ErrorOrPtr unload_driver(dev_url_t url) {

  int error;
  dev_manifest_t* manifest;

  manifest = hive_get(__installed_driver_manifests, url);

  if (!manifest)
    return Error();

  if (!verify_driver(manifest))
    return Error();

  mutex_lock(manifest->m_lock);

  error = NULL; 

  /* Exit */
  if (manifest->m_handle->f_exit)
    error = manifest->m_handle->f_exit();

  /* Clear the loaded flag */
  manifest->m_flags &= ~DRV_LOADED;

  /* Unregister presence before unlocking the manifest */
  __driver_unregister_presence(manifest->m_handle->m_type);
  __driver_unregister_active(manifest->m_handle->m_type, manifest); 

  if (manifest->m_external && (manifest->m_flags & DRV_IS_EXTERNAL) == DRV_IS_EXTERNAL)
    unload_external_driver(manifest->m_external);

  mutex_unlock(manifest->m_lock);

  /* Fuck man */
  if (error)
    return Error();

  return Success(0);
}


bool is_driver_loaded(dev_manifest_t* manifest) {
  return (is_driver_installed(manifest) && (manifest->m_flags & DRV_LOADED) == DRV_LOADED);
}


bool is_driver_installed(dev_manifest_t* manifest) {
  if (!__installed_driver_manifests || !manifest) {
    return false;
  }

  return (hive_get(__installed_driver_manifests, manifest->m_url) != nullptr);
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

  if (!__installed_driver_manifests || !url)
    return nullptr;

  /* If we are asking for the current executing driver */
  if (url && strcmp(url, "this") == 0) {
    /* TODO */
    return nullptr;
  }

  return hive_get(__installed_driver_manifests, url);
}

size_t get_driver_type_count(dev_type_t type)
{
  return __dev_constraints[type].current_count;
}

struct dev_manifest* get_active_driver_from_type(dev_type_t type)
{
  dev_constraint_t constraint = __dev_constraints[type];
  return constraint.active;
}

int set_active_driver(struct dev_manifest* dev, dev_type_t type)
{
  dev_constraint_t* constraint = &__dev_constraints[type];

  if (constraint->active)
    return -1;

  mutex_lock(__driver_constraint_lock);
  constraint->active = dev;
  mutex_unlock(__driver_constraint_lock);

  return 0;
}

/*!
 * @brief Copy the path of the driver url into a buffer
 *
 * The URL (we use path and URL together for the same stuff) of any driver will 
 * probably never exceed 128 bytes. If they eventually do, we're fucked =)
 *
 * @buffer: the buffer we copy to
 * @type: the driver type we want to get the path of
 */
int get_active_driver_path(char buffer[128], dev_type_t type)
{
  dev_manifest_t* manifest;
  dev_constraint_t* constraint;

  if (!buffer)
    return -1;

  constraint = &__dev_constraints[type];

  manifest = constraint->active;

  if (!manifest)
    return -1;

  if (strlen(manifest->m_url) >= 128)
    return -2;

  memcpy(buffer, manifest->m_url, strlen(manifest->m_url) + 1);

  return 0;
}

/*!
 * @brief Find the nth driver of a type
 *
 *
 * @type: the type of driver to look for
 * @index: the index into the list of drivers there
 *
 * @returns: the manifest of the driver we find
 */
struct dev_manifest* get_driver_from_type(dev_type_t type, uint32_t index)
{
  const char* url_start = dev_type_urls[type];
  dev_constraint_t constraint = __dev_constraints[type];

  if (index >= constraint.current_count)
    index = 0;

  /* Manual hive itteration LMAO */
  FOREACH(i, __installed_driver_manifests->m_entries) {
    hive_entry_t* entry = i->data;

    if (entry->m_hole && strcmp(entry->m_entry_part, url_start) == 0) {

      FOREACH(j, entry->m_hole->m_entries) {
        hive_entry_t* entry = j->data;

        if (!entry->m_data)
          continue;

        if (!index)
          return entry->m_data;

        index--;
      }
    }
  }

  return nullptr;
}

/*
 * Replaces the current active driver of this manifests type
 * with the manifests. This unloads the old driver and loads the 
 * new one. Both must be installed
 *
 * @manifest: the manifest to try to activate
 * @uninstall: if true we uninstall the old driver
 */
void replace_active_driver(struct dev_manifest* manifest, bool uninstall)
{
  kernel_panic("TODO: replace_active_driver");
}

/*!
 * @brief Register a core driver for a given type
 *
 * Fails if there already is a core driver for this type. Core drivers should abort their 
 * load when this fails
 *
 * @driver: the driver to register as a core driver
 * @type: the type to register a core driver for
 */
int register_core_driver(struct aniva_driver* driver, dev_type_t type)
{
  dev_url_t url;
  dev_manifest_t* manifest;
  dev_constraint_t* constraint;

  if (!driver || !VALID_DEV_TYPE(type))
    return -1;

  constraint = &__dev_constraints[type];

  if (constraint->core)
    return -2;

  url = get_driver_url(driver);

  if (!url)
    return -3;

  manifest = get_driver(url);

  if (!manifest) {
    kfree((void*)url);
    return -4;
  }

  mutex_lock(__core_driver_lock);

  constraint->core = manifest;

  mutex_unlock(__core_driver_lock);
  kfree((void*)url);
  return 0;
}

/*!
 * @brief Unregister a driver as a core driver
 *
 * Fails if this driver is not a core driver
 *
 * @driver: the driver to unregister
 */
int unregister_core_driver(struct aniva_driver* driver)
{
  dev_url_t url;
  dev_manifest_t* manifest;
  dev_constraint_t* constraint;

  if (!driver)
    return -1;

  url = get_driver_url(driver);
  manifest = get_driver(url);

  if (!manifest) {
    kfree((void*)url);
    return -2;
  }

  kfree((void*)url);

  /* Lock the mutex to prevent any weirdness */
  mutex_lock(__core_driver_lock);

  for (uint32_t i = 0; i < DRIVER_TYPE_COUNT; i++) {
    constraint = &__dev_constraints[i];

    /* Check if this manifest is contained somewhere as a core driver */
    if (constraint->core == manifest) {
      constraint->core = nullptr;

      mutex_unlock(__core_driver_lock);
      return 0;
    }
  }

  mutex_unlock(__core_driver_lock);
  return -3;
}

dev_manifest_t* try_driver_get(aniva_driver_t* driver, uint32_t flags) {

  dev_url_t path;
  dev_manifest_t* ret;

  if (!driver) {
    return nullptr;
  }

  path = get_driver_url(driver);

  ret = get_driver(path);

  kfree((void*)path);

  if (!ret)
    goto fail_and_exit;

  /* If the flags specify that the driver has to be active, we check for that */
  if ((ret->m_flags & DRV_ACTIVE) == 0 && (flags & DRV_ACTIVE) == DRV_ACTIVE)
    goto fail_and_exit;

  return ret;

fail_and_exit:
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
  dev_manifest_t* manifest = hive_get(__installed_driver_manifests, path);

  /* Check if the manifest is loaded */
  if (!manifest || (manifest->m_flags & DRV_LOADED) != DRV_LOADED)
    return Error();

  manifest->m_flags |= DRV_ACTIVE;

  return Success(0);
}

ErrorOrPtr foreach_driver(bool (*callback)(hive_t* h, void* manifset))
{
  return hive_walk(__installed_driver_manifests, true, callback);
}

/*!
 * @brief Send a packet to a driver directly
 *
 * This function is kinda funny, since it does not really send a packet in the traditional sense, but it
 * rather calls the drivers message function while also locking it. This is because drivers are pretty much static
 * most of the time and also not capable of async functions. For that we need a socket, which polls for packets every
 * so often. We might want to probe a driver to see if it has a socket but that will require more busses and
 * more registry...
 */
ErrorOrPtr driver_send_msg(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size) {
  return driver_send_msg_a(path, code, buffer, buffer_size, nullptr, NULL);
}

/*!
 * @brief: Sends a message to a driver with a response buffer
 *
 * Nothing to add here...
 */
ErrorOrPtr driver_send_msg_a(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t resp_buffer_size)
{
  dev_manifest_t* manifest;

  if (!path)
    return Error();

  manifest = hive_get(__installed_driver_manifests, path);

  /*
   * Only loaded drivers can recieve messages
   */
  if (!manifest || (manifest->m_flags & DRV_LOADED) != DRV_LOADED)
    return Error();

  return driver_send_msg_ex(manifest, code, buffer, buffer_size, resp_buffer, resp_buffer_size);
}


/*
 * When resp_buffer AND resp_buffer_size are non-null, we assume the caller wants to
 * take in a direct response, in which case we can copy the response from the driver
 * directly into the buffer that the caller gave us
 */
ErrorOrPtr driver_send_msg_ex(struct dev_manifest* manifest, dcc_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t resp_buffer_size) 
{
  uintptr_t error;
  aniva_driver_t* driver;

  if (!manifest)
    return Error();

  driver = manifest->m_handle;

  if (!driver->f_msg) 
    return Error();

  mutex_lock(manifest->m_lock);

  /* NOTE: it is the drivers job to verify the buffers. We don't manage shit here */
  error = driver->f_msg(manifest->m_handle, code, buffer, buffer_size, resp_buffer, resp_buffer_size);

  mutex_unlock(manifest->m_lock);

  if (error)
    return ErrorWithVal(error);

  return Success(0);
}


/*
 * NOTE: this function leaves behind a dirty packet response.
 * It is left to the caller to clean that up
 */
ErrorOrPtr driver_send_msg_sync(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size) {
  return driver_send_msg_sync_with_timeout(path, code, buffer, buffer_size, DRIVER_WAIT_UNTIL_READY);
}


#define LOCK_SOCKET_MAYBE(socket) do { if (socket) mutex_lock(socket->m_packet_mutex); } while(0)
#define UNLOCK_SOCKET_MAYBE(socket) do { if (socket) mutex_unlock(socket->m_packet_mutex); } while(0)

ErrorOrPtr driver_send_msg_sync_with_timeout(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, size_t mto) {

  uintptr_t result;
  dev_manifest_t* manifest = hive_get(__installed_driver_manifests, path);

  if (!manifest || (manifest->m_flags & DRV_LOADED) != DRV_LOADED)
    goto exit_fail;

  /*
   * Invalid, or our driver can't be reached =/
   */
  if (!manifest->m_handle || !manifest->m_handle->f_msg)
    goto exit_fail;

  // TODO: validate checksums and funnie hashes
  // TODO: validate all dependencies are loaded

  // NOTE: this skips any verification by the packet multiplexer, and just 
  // kinda calls the drivers onPacket function whenever. This can result in
  // funny behaviour, so we should try to take the drivers packet mutex, in order 
  // to ensure we are the only ones asking this driver to handle some data

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

  mutex_lock(manifest->m_lock);

  result = manifest->m_handle->f_msg(manifest->m_handle, code, buffer, buffer_size, NULL, NULL);

  mutex_unlock(manifest->m_lock);

  if (result)
    return Error();

  return Success(0);

exit_fail:
  return Error();
}
