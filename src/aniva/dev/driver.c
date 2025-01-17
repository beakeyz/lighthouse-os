#include "driver.h"
#include "dev/device.h"
#include "dev/driver.h"
#include "dev/loader.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/api/device.h"
#include "lightos/api/objects.h"
#include "logging/log.h"
#include "mem/kmem.h"
#include "mem/phys.h"
#include "mem/tracker/tracker.h"
#include "oss/object.h"
#include "sync/mutex.h"
#include "system/profile/profile.h"
#include "system/sysvar/var.h"
#include <libk/string.h>
#include <mem/heap.h>

#define DRIVER_RESOURCES_INITIAL_TRACKER_BUFSZ 0x8000

error_t init_driver_resources(driver_resources_t* resources)
{
    error_t error;
    void* range_cache_buf;

    /* Clear the resources buffer */
    memset(resources, 0, sizeof(*resources));

    /* Allocate a cache range for this tracker */
    error = kmem_kernel_alloc_range(&range_cache_buf, 0x8000, NULL, KMEM_FLAG_WRITABLE);

    if (error)
        return error;

    /* Initialize the page tracker of this driver */
    error = init_page_tracker(&resources->tracker, range_cache_buf, DRIVER_RESOURCES_INITIAL_TRACKER_BUFSZ, (u64)-1);

    if (error) {
        kmem_kernel_dealloc((u64)range_cache_buf, resources->tracker.sz_cache_buffer);
        return error;
    }

    /* Put these bytes on the bill of this driver */
    resources->total_allocated_bytes = resources->tracker.sz_cache_buffer;

    return 0;
}

error_t destroy_driver_resources(driver_resources_t* resources)
{
    size_t bsize;
    void* buffer;

    if (!resources)
        return -EINVAL;

    /* Cache this size real quick */
    bsize = resources->tracker.sz_cache_buffer;
    buffer = resources->tracker.cache_buffer;

    /* Deallocate all the memory from this tracker */
    ASSERT(IS_OK(kmem_phys_dealloc_from_tracker(nullptr, &resources->tracker)));

    /* Fuck */
    destroy_page_tracker(&resources->tracker);

    /* Yeet out the memory */
    return kmem_kernel_dealloc((u64)buffer, bsize);
}

int generic_driver_entry(driver_t* driver);

bool driver_is_ready(driver_t* driver)
{
    const bool active_flag = (driver->m_flags & DRV_ACTIVE) == DRV_ACTIVE;

    return active_flag && !mutex_is_locked(driver->m_lock);
}

bool driver_is_busy(driver_t* driver)
{
    if (!driver)
        return false;

    return (mutex_is_locked(driver->m_lock));
}

/*
 * Quick TODO: create a way to validate pointer origin
 */
int drv_read(driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset)
{
    if (!driver || !driver->m_ctl_device)
        return -KERR_INVAL;

    /* Let's hope this is real lmaooo */
    if (!buffer_size)
        return -KERR_INVAL;

    return device_send_ctl_ex(driver->m_ctl_device, DEVICE_CTLC_READ, offset, buffer, *buffer_size);
}

int drv_write(driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset)
{
    if (!driver || !driver->m_ctl_device)
        return -KERR_INVAL;

    /* Let's hope this is real lmaooo */
    if (!buffer_size)
        return -KERR_INVAL;

    return device_send_ctl_ex(driver->m_ctl_device, DEVICE_CTLC_WRITE, offset, buffer, *buffer_size);
}

/*!
 * @brief: Take over a device which has no driver yet
 */
int driver_takeover_device(struct driver* driver, struct device* device, const char* newname, struct dgroup* newgroup, void* dev_priv)
{
    int error;

    if (!driver || !device || !newname)
        return -KERR_INVAL;

    if (device_has_driver(device))
        return -KERR_DRV;

    mutex_lock(device->lock);

    error = device_rename(device, newname);

    if (error)
        goto unlock_and_exit;

    /* Move this ahci device to the correct bus device */
    if (newgroup) {
        error = device_move(device, newgroup);

        if (error)
            goto unlock_and_exit;
    }

    /* Now bind the driver */
    error = device_bind_driver(device, driver);

    if (error)
        goto unlock_and_exit;

    /* Set the devices private field */
    device->private = dev_priv;

unlock_and_exit:
    mutex_unlock(device->lock);
    return error;
}

int generic_driver_entry(driver_t* driver)
{
    if ((driver->m_flags & DRV_ACTIVE) == DRV_ACTIVE)
        return -1;

    /* NOTE: The driver can also mark itself as ready */
    int error = driver->m_handle->f_init(driver);

    if (!error) {
        /* Init finished, the driver is ready for messages */
        driver->m_flags |= DRV_ACTIVE;
    } else {
        driver->m_flags |= DRV_FAILED;
    }

    /* Get any failures through to the bootstrap */
    return error;
}

kerror_t bootstrap_driver(driver_t* driver)
{
    int error;

    if (!driver)
        return -1;

    if (driver->m_flags & DRV_FS) {
        /* TODO: redo */
    }

    /* Preemptively set the driver to inactive */
    driver->m_flags &= ~(DRV_ACTIVE | DRV_FAILED);

    // NOTE: if the drivers port is not valid, the subsystem will verify
    // it and provide a new port, so we won't have to wory about that here
    error = generic_driver_entry(driver);

    if (error)
        return -1;

    return (0);
}

/*
 * A single driver can't manage more devices than this. If for some reason this number is
 * exceeded for good reason, we should create multiplexer drivers or something
 */
#define DM_MAX_DEVICES 512

size_t get_driver_url_length(aniva_driver_t* handle)
{
    const char* driver_type_url = get_driver_type_url(handle->m_type);
    return strlen(driver_type_url) + 1 + strlen(handle->m_name);
}

const char* get_driver_url(aniva_driver_t* handle)
{
    /* Prerequisites */
    const char* dtu = get_driver_type_url(handle->m_type);
    size_t dtu_length = strlen(dtu);

    size_t size = get_driver_url_length(handle);
    const char* ret = kmalloc(size + 1);
    memset((char*)ret, 0, size + 1);
    memcpy((char*)ret, dtu, dtu_length);
    *(char*)&ret[dtu_length] = DRIVER_URL_SEPERATOR;
    memcpy((char*)ret + dtu_length + 1, handle->m_name, strlen(handle->m_name));

    return ret;
}

static void __destroy_driver(driver_t* driver)
{
    destroy_mutex(driver->m_lock);

    /* A driver might exist without any dependencies */
    if (driver->m_dep_list)
        destroy_vector(driver->m_dep_list);

    /*
     * Murder the vector for devices
     * TODO/FIXME: What do we do with attached devices when
     * a driver gets destroyed?
     */
    destroy_list(driver->m_dev_list);

    /* Kill off our resource tracker */
    destroy_driver_resources(&driver->resources);

    /* Free strings and stuff */
    kfree((void*)driver->m_url);
    if (driver->m_image_path)
        kfree((void*)driver->m_image_path);

    free_ddriver(driver);
}

static int __destroy_driver_oss(oss_object_t* object)
{
    driver_t* driver = oss_object_unwrap(object, OT_DRIVER);

    if (!driver)
        return -EINVAL;

    __destroy_driver(driver);

    return 0;
}

static oss_object_ops_t driver_oss_ops = {
    .f_Destroy = __destroy_driver_oss,
};

driver_t* create_driver(aniva_driver_t* handle)
{
    driver_t* ret;

    ret = allocate_ddriver();

    ASSERT(ret);

    memset(ret, 0, sizeof(*ret));

    ret->m_lock = create_mutex(NULL);
    ret->m_handle = handle;

    ret->m_flags = NULL;
    ret->m_dep_count = NULL;
    ret->m_dep_list = NULL;

    if (handle) {
        ret->m_check_version = handle->m_version;
        // TODO: concat
        ret->m_url = get_driver_url(handle);

        ret->m_obj = create_oss_object(handle->m_name, NULL, OT_DRIVER, &driver_oss_ops, ret);
    } else {
        ret->m_flags |= DRV_DEFERRED_HNDL;
    }

    /* Initialize our resource tracker */
    init_driver_resources(&ret->resources);

    ret->m_max_n_dev = DRIVER_MAX_DEV_COUNT;
    // ret->m_dev_list = create_vector(ret->m_max_n_dev, sizeof(device_t*), VEC_FLAG_NO_DUPLICATES);
    ret->m_dev_list = init_list();

    return ret;
}

kerror_t driver_emplace_handle(driver_t* driver, aniva_driver_t* handle)
{
    if (driver->m_handle || (driver->m_flags & DRV_DEFERRED_HNDL) != DRV_DEFERRED_HNDL)
        return -1;

    ASSERT_MSG(driver->m_obj == nullptr || strcmp(driver->m_obj->key, handle->m_name) == 0, "Tried to emplace a handle on a driver which already has an object");

    /* Mark the driver as non-deferred */
    driver->m_flags &= ~DRV_DEFERRED_HNDL;
    driver->m_flags |= DRV_HAS_HANDLE;

    /* Emplace the handle and its data */
    driver->m_handle = handle;
    driver->m_check_version = handle->m_version;
    driver->m_url = get_driver_url(handle);

    if (!driver->m_obj)
        driver->m_obj = create_oss_object(handle->m_name, NULL, OT_DRIVER, &driver_oss_ops, driver);
    return (0);
}

/*!
 * @brief Destroy a driver and deallocate it's resources
 *
 * This assumes that the underlying driver has already been taken care of
 *
 * NOTE: does not destroy the devices on this driver. Caller needs to make sure this is
 * done. The driver behind this driver needs to implement functions that handle the propper
 * destruction of the devices, since it is the one who owns this memory
 */
void destroy_driver(driver_t* driver)
{
    if (driver->m_obj)
        oss_object_close(driver->m_obj);
    else
        __destroy_driver(driver);
}

static driver_t* _get_dependency_from_path(drv_dependency_t* dep)
{
    kerror_t error;
    char* abs_path;
    size_t abs_path_len;
    const char* relative_path;
    sysvar_t* drv_root;
    driver_t* ret;

    if (dep->type != DRV_DEPTYPE_PATH)
        return nullptr;

    if ((dep->flags & DRVDEP_FLAG_RELPATH) != DRVDEP_FLAG_RELPATH)
        return install_external_driver(dep->location);

    error = profile_find_var(DRIVERS_LOC_VAR_PATH, &drv_root);

    if (error)
        return nullptr;

    /* We only need to read the pointer of this str, so lock the var */
    sysvar_lock(drv_root);

    /* Read the relative path by leaking the reference of the vars string */
    relative_path = sysvar_read_str(drv_root);

    /* Calculate a new length */
    abs_path_len = strlen(relative_path) + strlen(dep->location) + 1;
    /* Allocate a region for this */
    abs_path = kmalloc(abs_path_len);

    memset(abs_path, 0, abs_path_len);
    /* Construct the full path */
    sfmt_sz(abs_path, abs_path_len, "%s%s", relative_path, dep->location);

    /* Unlock it again */
    sysvar_unlock(drv_root);

    /* Release the reference to this sysvar */
    release_sysvar(drv_root);

    /* Try to find the driver */
    ret = install_external_driver(abs_path);

    /* Clear the path */
    kfree((void*)abs_path);

    return ret;
}

/*!
 * @brief: 'serialize' the given driver paths into the driver
 *
 * This ONLY gathers the dependencies. It does not do any driver loading
 */
int driver_gather_dependencies(driver_t* driver)
{
    driver_dependency_t man_dep;
    aniva_driver_t* handle = driver->m_handle;

    /* We should not have any dependencies on the driver at this point */
    ASSERT(!driver->m_dep_count);
    ASSERT(!driver->m_dep_list);

    if (!handle->m_deps || !handle->m_deps->location)
        return KERR_INVAL;

    /* Count the dependencies */
    while (handle->m_deps[driver->m_dep_count].location)
        driver->m_dep_count++;

    if (!driver->m_dep_count)
        return KERR_INVAL;

    driver->m_dep_list = create_vector(driver->m_dep_count, sizeof(driver_dependency_t), NULL);

    /*
     * We kinda trust too much in the fact that m_dep_count is
     * correct...
     */
    for (uintptr_t i = 0; i < driver->m_dep_count; i++) {

        /* TODO: we can check if this address is located in a
         used resource in oder to validate it */
        drv_dependency_t* drv_dep = &handle->m_deps[i];

        if (!drv_dep->location)
            return -KERR_INVAL;

        /* Clear */
        memset(&man_dep, 0, sizeof(man_dep));

        /* Copy over the raw driver dependency */
        memcpy(&man_dep.dep, drv_dep, sizeof(*drv_dep));

        /* Try to resolve the dependency */
        switch (drv_dep->type) {
        case DRV_DEPTYPE_URL:
            man_dep.obj.drv = get_driver(drv_dep->location);
            break;
        case DRV_DEPTYPE_PATH:
            man_dep.obj.drv = _get_dependency_from_path(drv_dep);

            /* If this was an essential dependency, we bail */
            if (!man_dep.obj.drv && !drv_dep_is_optional(drv_dep))
                return -KERR_INVAL;

            break;
        default:
            printf("Got dependency type: %d with location %s\n", drv_dep->type, drv_dep->location);
            kernel_panic("TODO: implement driver dependency type");
            break;
        }

        /* Copy the dependency into the vector */
        vector_add(driver->m_dep_list, &man_dep, NULL);
    }

    return 0;
}

int driver_add_dev(struct driver* driver, device_t* device)
{
    kerror_t error = 0;

    if (!driver || !device)
        return -1;

    mutex_lock(driver->m_lock);

    /* Check if we're still under the max */
    if (driver->m_dev_list->m_length < driver->m_max_n_dev)
        list_append(driver->m_dev_list, device);
    else
        error = -KERR_NOMEM;

    mutex_unlock(driver->m_lock);

    return error;
}

int driver_remove_dev(struct driver* driver, device_t* device)
{
    int error = -KERR_NOT_FOUND;

    if (!driver || !device)
        return -1;

    mutex_lock(driver->m_lock);

    /* Try to remove the bitch */
    if (!list_remove_ex(driver->m_dev_list, device))
        goto unlock_and_exit;

    error = 0;
unlock_and_exit:
    mutex_unlock(driver->m_lock);
    return error;
}

/*!
 * @brief: Check if @driver is responsible for @device
 *
 */
bool driver_has_dev(struct driver* driver, struct device* device, uint32_t* p_idx)
{
    kerror_t error;

    mutex_lock(driver->m_lock);

    error = list_indexof(driver->m_dev_list, p_idx, device);

    mutex_unlock(driver->m_lock);

    /* If we where able to find an index, the driver has this device */
    return (error == 0);
}

/*!
 * @brief: Sets the control device object for a driver
 *
 * Also registers the device to the device root (%/Dev). Fails if
 * there is already a device object attached there with the same name
 */
int driver_set_ctl_device(struct driver* driver, struct device* device)
{
    kerror_t error;

    if (!driver || !device)
        return -KERR_INVAL;

    error = -KERR_DUPLICATE;

    mutex_lock(driver->m_lock);

    /* The current control device needs to be destroyed first... */
    if (driver->m_ctl_device)
        goto unlock_and_exit;

    /* Register the control devices to the device root */
    error = device_register(device, NULL);

    /* Check if we did it */
    if (error)
        goto unlock_and_exit;

    /* Set the control device */
    driver->m_ctl_device = device;

    /* Reset error lol */
    error = 0;
unlock_and_exit:
    mutex_unlock(driver->m_lock);
    return error;
}

/*!
 * @brief: Destroys the current control device of a driver
 */
int driver_destroy_ctl_device(struct driver* driver)
{
    if (!driver || !driver->m_lock)
        return -KERR_INVAL;

    mutex_lock(driver->m_lock);

    if (!driver->m_ctl_device)
        goto unlock_and_exit;

    /* Destroy the drivers control device */
    destroy_device(driver->m_ctl_device);

    /* Rest the ctl device pointer */
    driver->m_ctl_device = NULL;
unlock_and_exit:
    mutex_unlock(driver->m_lock);
    return 0;
}
