#include "core.h"
#include "dev/driver.h"
#include "driver.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "logging/log.h"
#include "mem/zalloc/zalloc.h"
#include "oss/core.h"
#include "oss/obj.h"
#include "proc/core.h"
#include "proc/thread.h"
#include "sched/scheduler.h"
#include "sync/mutex.h"
#include "system/processor/processor.h"
#include <entry/entry.h>
#include <mem/heap.h>
#include <oss/node.h>

static oss_node_t* __driver_node;
static zone_allocator_t* __driver_allocator;

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
    [DT_FIRMWARE] = "fw",
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

const char* driver_get_type_str(struct driver* driver)
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
        0 },
    [DT_FS] = { .type = DT_FS, .max_count = DRV_INFINITE, .max_active = DRV_INFINITE, 0 },
    [DT_IO] = { .type = DT_IO, .max_count = DRV_INFINITE, .max_active = DRV_INFINITE, 0 },
    [DT_SOUND] = { .type = DT_SOUND, .max_count = 10, .max_active = 1, 0 },
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
    [DT_GRAPHICS] = { .type = DT_GRAPHICS, .max_count = 10, .max_active = 2, 0 },
    [DT_OTHER] = { .type = DT_OTHER, .max_count = DRV_INFINITE, .max_active = DRV_INFINITE, 0 },
    [DT_DIAGNOSTICS] = { .type = DT_DIAGNOSTICS, .max_count = 1, .max_active = 1, 0 },
    [DT_SERVICE] = { .type = DT_SERVICE, .max_count = DRV_SERVICE_MAX, .max_active = DRV_SERVICE_MAX, 0 },
    [DT_FIRMWARE] = { .type = DT_FIRMWARE, .max_count = DRV_INFINITE, .max_active = DRV_INFINITE, 0 }
};

static list_t* __deferred_driver_drivers;

static bool __load_precompiled_driver(driver_t* driver)
{
    if (!driver)
        return false;

    /* Trust that this will be loaded later */
    if (driver->m_flags & DRV_DEFERRED)
        return true;

    /* Skip already loaded drivers */
    if ((driver->m_flags & DRV_LOADED) == DRV_LOADED)
        return true;

    driver_gather_dependencies(driver);

    /*
     * NOTE: this fails if the driver is already loaded, but we ignore this
     * since we also load drivers preemptively if they are needed as a dependency
     */
    load_driver(driver);

    return true;
}

static bool walk_precompiled_drivers_to_load(oss_node_t* node, oss_obj_t* data, void* arg)
{
    driver_t* driver;

    /* Walk recursively */
    if (node)
        return !oss_node_itterate(node, walk_precompiled_drivers_to_load, arg);

    driver = oss_obj_unwrap(data, driver_t);

    return __load_precompiled_driver(driver);
}

driver_t* allocate_ddriver()
{
    driver_t* ret;

    if (!__driver_allocator)
        return nullptr;

    ret = zalloc_fixed(__driver_allocator);

    memset(ret, 0, sizeof(driver_t));

    return ret;
}

void free_ddriver(struct driver* driver)
{
    if (!__driver_allocator)
        return;

    zfree_fixed(__driver_allocator, driver);
}

/*!
 * @brief Verify the validity of a driver and also if it is clear to be loaded
 *
 * @returns: false on failure (the driver is invalid or not clear to be loaded)
 * true on success (driver is valid)
 */
bool verify_driver(driver_t* driver)
{
    dev_constraint_t* constaint;
    aniva_driver_t* anv_driver;

    /* External driver are considered valid here */
    if ((driver->m_flags & DRV_IS_EXTERNAL) == DRV_IS_EXTERNAL)
        return true;

    anv_driver = driver->m_handle;

    if (!driver || !anv_driver->f_init)
        return false;

    if (anv_driver->m_type >= DRIVER_TYPE_COUNT)
        return false;

    constaint = &__dev_constraints[anv_driver->m_type];

    /* We can't load more of this type of driver, so we mark this as invalid */
    if (constaint->current_count == constaint->max_count)
        return false;

    return true;
}

static bool __should_defer(driver_t* driver)
{
    /* TODO: */
    (void)driver;
    return false;
}

kerror_t install_driver(driver_t* driver)
{
    int error;

    if (!driver)
        goto fail_and_exit;

    if (!verify_driver(driver))
        goto fail_and_exit;

    if (is_driver_installed(driver))
        goto fail_and_exit;

    error = oss_attach_obj_rel(__driver_node, driver->m_url, driver->m_obj);

    if (error)
        goto fail_and_exit;

    /* Mark this driver as deferred, so that we can delay its loading */
    if (__deferred_driver_drivers && __should_defer(driver)) {
        list_append(__deferred_driver_drivers, driver);
        driver->m_flags |= DRV_DEFERRED;
    }

    return (0);

fail_and_exit:
    if (driver)
        destroy_driver(driver);
    return -1;
}

/*!
 * @brief Detaches the drivers path from the driver tree and destroys the driver
 *
 * NOTE: We don't detach the obj here, since that's done in destroy_driver
 * TODO: resolve dependencies!
 */
kerror_t uninstall_driver(driver_t* driver)
{
    if (!driver)
        return -1;

    /* Uninstalled stuff can't be uninstalled */
    if (!is_driver_installed(driver))
        goto fail_and_exit;

    /* When we fail to unload something, thats quite bad lmao */
    if (is_driver_loaded(driver) && (unload_driver(driver->m_url)))
        goto fail_and_exit;

    destroy_driver(driver);

    return (0);

fail_and_exit:
    // kfree((void*)driver_url);
    return -1;
}

static void __driver_register_presence(enum DRIVER_TYPE type)
{
    if (type >= DRIVER_TYPE_COUNT)
        return;

    mutex_lock(__driver_constraint_lock);
    __dev_constraints[type].current_count++;
    mutex_unlock(__driver_constraint_lock);
}

static void __driver_unregister_presence(enum DRIVER_TYPE type)
{
    if (type >= DRIVER_TYPE_COUNT || __dev_constraints[type].current_count)
        return;

    mutex_lock(__driver_constraint_lock);
    __dev_constraints[type].current_count--;
    mutex_unlock(__driver_constraint_lock);
}

/*
 * Steps to load a driver into our registry
 * 1: Resolve the url in the driver
 * 2: Validate the driver using its driver
 * 3: emplace the driver into the drivertree
 * 4: run driver bootstraps
 *
 * We also might want to create a kernel-process for each driver
 * TODO: better security
 */
kerror_t load_driver(driver_t* driver)
{
    kerror_t error;
    aniva_driver_t* handle;

    if (!driver)
        return -1;

    if (!verify_driver(driver))
        goto fail_and_exit;

    handle = driver->m_handle;

    /* Can't load if it's already loaded lmao */
    if (is_driver_loaded(driver))
        return 0;

    // TODO: we can use ANIVA_FAIL_WITH_WARNING here, but we'll need to refactor some things
    // where we say result == ANIVA_FAIL
    // these cases will return false if we start using ANIVA_FAIL_WITH_WARNING here, so they will
    // need to be replaced with result != ANIVA_SUCCESS
    if (!is_driver_installed(driver) && (install_driver(driver)))
        goto fail_and_exit;

    KLOG_INFO("Loading driver: %s\n", handle->m_name);

    if (!driver->m_dep_list)
        goto skip_dependencies;

    FOREACH_VEC(driver->m_dep_list, data, idx)
    {
        driver_dependency_t* dep = (driver_dependency_t*)data;

        /* Skip non-driver dependencies for now */
        ASSERT_MSG(drv_dep_is_driver(&dep->dep), "TODO: implement non-driver dependencies for drivers");

        // TODO: check for errors
        if (dep && !is_driver_loaded(dep->obj.drv)) {
            KLOG_INFO("Loading driver dependency: %s\n", dep->obj.drv->m_url);

            if (driver_is_deferred(dep->obj.drv))
                kernel_panic("TODO: handle deferred dependencies!");

            if (load_driver(dep->obj.drv))
                goto fail_and_exit;
        }
    }

skip_dependencies:
    error = bootstrap_driver(driver);

    /* If the driver says something went wrong, trust that */
    if (error || (driver->m_flags & DRV_FAILED)) {
        unload_driver(driver->m_url);
        return -1;
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
    //__driver_register_active(handle->m_type, driver);

    driver->m_flags |= DRV_LOADED;

    return (0);

fail_and_exit:
    return -1;
}

kerror_t unload_driver(dev_url_t url)
{
    int error;
    driver_t* driver;
    oss_obj_t* obj;

    error = oss_resolve_obj_rel(__driver_node, url, &obj);

    if (error && !obj)
        return -1;

    driver = oss_obj_unwrap(obj, driver_t);

    if (!verify_driver(driver))
        return -1;

    mutex_lock(driver->m_lock);

    error = NULL;

    /* Exit */
    if (driver->m_handle->f_exit)
        error = driver->m_handle->f_exit();

    /* Clear the loaded flag */
    driver->m_flags &= ~DRV_LOADED;

    /* Unregister presence before unlocking the driver */
    __driver_unregister_presence(driver->m_handle->m_type);

    mutex_unlock(driver->m_lock);

    /* Fuck man */
    if (error)
        return -1;

    return (0);
}

bool is_driver_loaded(driver_t* driver)
{
    return (is_driver_installed(driver) && (driver->m_flags & DRV_LOADED) == DRV_LOADED);
}

bool is_driver_installed(driver_t* driver)
{
    bool is_installed;
    oss_obj_t* entry;

    if (!__driver_node || !driver)
        return false;

    entry = nullptr;

    is_installed = (oss_resolve_obj_rel(__driver_node, driver->m_url, &entry) == 0 && entry != nullptr);

    (void)oss_obj_close(entry);

    return is_installed;
}

/*
 * TODO: when checking this, it could be that the url
 * is totally invalid and results in a pagefault. In that
 * case we want to be able to tell the pf handler that we
 * can expect a pagefault and that when it happens, we should
 * not terminate the kernel, but rather signal it to this
 * routine so that we can act accordingly
 */
driver_t* get_driver(dev_url_t url)
{
    int error;
    oss_obj_t* obj;

    if (!__driver_node || !url)
        return nullptr;

    error = oss_resolve_obj_rel(__driver_node, url, &obj);

    if (error || !obj)
        return nullptr;

    /* driver should be packed into the object */
    return oss_obj_unwrap(obj, driver_t);
}

size_t get_driver_type_count(enum DRIVER_TYPE type)
{
    return __dev_constraints[type].current_count;
}

/*!
 * @brief Find the nth driver of a type
 *
 *
 * @type: the type of driver to look for
 * @index: the index into the list of drivers there
 *
 * @returns: the driver of the driver we find
 */
struct driver* get_driver_from_type(enum DRIVER_TYPE type, uint32_t index)
{
    kernel_panic("TODO: get_driver_from_type");
    return nullptr;
}

struct __drv_from_addr_info {
    driver_t* result;
    vaddr_t addr;
};

static bool __get_drv_from_addr_cb(oss_node_t* node, oss_obj_t* obj, void* arg)
{
    driver_t* driver;
    struct __drv_from_addr_info* info = arg;

    if (node)
        return !oss_node_itterate(node, __get_drv_from_addr_cb, arg);

    if (!obj)
        return true;

    /* Grab the driver */
    driver = oss_obj_unwrap(obj, driver_t);

    /* Check if this object even is a driver */
    if (obj->type != OSS_OBJ_TYPE_DRIVER || !driver)
        return true;

    /* If this address is not in the drivers range, go next */
    if (info->addr < driver->load_base || info->addr > (driver->load_base + driver->load_size))
        return true;

    /* Found our driver, return */
    info->result = driver;
    return false;
}

struct driver* get_driver_from_address(vaddr_t addr)
{
    struct __drv_from_addr_info info;

    /* Fill the result buffer */
    info.addr = addr;
    info.result = nullptr;

    /* Call the itteration */
    (void)foreach_driver(__get_drv_from_addr_cb, &info);

    return info.result;
}

driver_t* try_driver_get(aniva_driver_t* driver, uint32_t flags)
{
    dev_url_t path;
    driver_t* ret;

    if (!driver)
        return nullptr;

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
kerror_t driver_set_ready(const char* path)
{
    // Fetch from the loaded drivers here, since unloaded
    // Drivers can never accept packets
    driver_t* driver = get_driver(path);

    /* Check if the driver is loaded */
    if (!driver || (driver->m_flags & DRV_LOADED) != DRV_LOADED)
        return -1;

    driver->m_flags |= DRV_ACTIVE;

    return (0);
}

kerror_t foreach_driver(bool (*callback)(oss_node_t* h, oss_obj_t* obj, void* arg0), void* arg0)
{
    if (oss_node_itterate(__driver_node, callback, arg0))
        return -1;

    return (0);
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
kerror_t driver_send_msg(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size)
{
    return driver_send_msg_a(path, code, buffer, buffer_size, nullptr, NULL);
}

/*!
 * @brief: Sends a message to a driver with a response buffer
 *
 * Nothing to add here...
 */
kerror_t driver_send_msg_a(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t resp_buffer_size)
{
    driver_t* driver;

    if (!path)
        return -1;

    driver = get_driver(path);

    /*
     * Only loaded drivers can recieve messages
     */
    if (!driver || (driver->m_flags & DRV_LOADED) != DRV_LOADED)
        return -1;

    return driver_send_msg_ex(driver, code, buffer, buffer_size, resp_buffer, resp_buffer_size);
}

/*
 * When resp_buffer AND resp_buffer_size are non-null, we assume the caller wants to
 * take in a direct response, in which case we can copy the response from the driver
 * directly into the buffer that the caller gave us
 */
kerror_t driver_send_msg_ex(struct driver* driver, dcc_t code, void* buffer, size_t buffer_size, void* resp_buffer, size_t resp_buffer_size)
{
    uintptr_t error;
    aniva_driver_t* anv_driver;

    if (!driver)
        return -1;

    anv_driver = driver->m_handle;

    if (!driver || !anv_driver->f_msg)
        return -1;

    // mutex_lock(driver->m_lock);

    /* NOTE: it is the drivers job to verify the buffers. We don't manage shit here */
    error = anv_driver->f_msg(driver->m_handle, code, buffer, buffer_size, resp_buffer, resp_buffer_size);

    // mutex_unlock(driver->m_lock);

    if (error)
        return error;

    return 0;
}

/*
 * NOTE: this function leaves behind a dirty packet response.
 * It is left to the caller to clean that up
 */
kerror_t driver_send_msg_sync(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size)
{
    return driver_send_msg_sync_with_timeout(path, code, buffer, buffer_size, DRIVER_WAIT_UNTIL_READY);
}

kerror_t driver_send_msg_sync_with_timeout(const char* path, driver_control_code_t code, void* buffer, size_t buffer_size, size_t mto)
{

    uintptr_t result;
    driver_t* driver = get_driver(path);

    if (!driver || (driver->m_flags & DRV_LOADED) != DRV_LOADED)
        goto exit_fail;

    /*
     * Invalid, or our driver can't be reached =/
     */
    if (!driver->m_handle || !driver->m_handle->f_msg)
        goto exit_fail;

    // TODO: validate checksums and funnie hashes
    // TODO: validate all dependencies are loaded

    // NOTE: this skips any verification by the packet multiplexer, and just
    // kinda calls the drivers onPacket function whenever. This can result in
    // funny behaviour, so we should try to take the drivers packet mutex, in order
    // to ensure we are the only ones asking this driver to handle some data

    size_t timeout = mto;

    /* NOTE: this is the same logic as that which is used in driver_is_ready(...) */
    while (!driver_is_ready(driver)) {

        scheduler_yield();

        if (timeout != DRIVER_WAIT_UNTIL_READY) {

            timeout--;
            if (timeout == 0) {
                goto exit_fail;
            }
        }
    }

    // mutex_lock(driver->m_lock);

    result = driver->m_handle->f_msg(driver->m_handle, code, buffer, buffer_size, NULL, NULL);

    // mutex_unlock(driver->m_lock);

    if (result)
        return -1;

    return (0);

exit_fail:
    return -1;
}

/*!
 * @brief: Initializes the in-kernel drivers
 */
void init_aniva_driver_registry()
{
    __driver_node = create_oss_node("Drv", OSS_OBJ_STORE_NODE, NULL, NULL);
    __driver_allocator = create_zone_allocator_ex(nullptr, NULL, driver_SOFTMAX * sizeof(driver_t), sizeof(driver_t), NULL);
    __deferred_driver_drivers = init_list();

    __driver_constraint_lock = create_mutex(NULL);
    __core_driver_lock = create_mutex(NULL);

    /*
     * Attach the node to the rootmap
     * Access is now granted through Drv/<type>/<name>
     */
    oss_attach_rootnode(__driver_node);

    FOREACH_CORE_DRV(ptr)
    {

        driver_t* driver;
        aniva_driver_t* anv_driver = *ptr;

        ASSERT_MSG(anv_driver, "Got an invalid precompiled driver! (ptr = NULL)");

        driver = create_driver(anv_driver);

        ASSERT_MSG(driver, "Failed to create driver for a precompiled driver!");

        ASSERT(install_driver(driver) == 0);
    }

    /* First load pass */
    oss_node_itterate(__driver_node, walk_precompiled_drivers_to_load, NULL);

    // Install exported drivers
    FOREACH_PCDRV(ptr)
    {

        driver_t* driver;
        aniva_driver_t* anv_driver = *ptr;

        ASSERT_MSG(anv_driver, "Got an invalid precompiled driver! (ptr = NULL)");

        driver = create_driver(anv_driver);

        ASSERT_MSG(driver, "Failed to create driver for a precompiled driver!");

        // NOTE: we should just let errors happen here,
        // since It could happen that a driver is already
        // loaded as a dependency. This means that this call
        // will always fail in that case, since we try to load
        // a driver that has already been loaded
        ASSERT(install_driver(driver) == 0);
    }

    /* Second load pass, with the core drivers already loaded */
    oss_node_itterate(__driver_node, walk_precompiled_drivers_to_load, NULL);

    FOREACH(i, __deferred_driver_drivers)
    {
        driver_t* driver = i->data;

        /* Skip invalid drivers, as a sanity check */
        if (!verify_driver(driver))
            continue;

        /* Clear the deferred flag */
        driver->m_flags &= ~DRV_DEFERRED;

        ASSERT_MSG(__load_precompiled_driver(driver), "Failed to load deferred precompiled driver!");
    }

    destroy_list(__deferred_driver_drivers);
    __deferred_driver_drivers = nullptr;
}

/*!
 * @brief: Start up the driver subsystem
 */
void init_driver_subsys()
{
    /* ... */
}
