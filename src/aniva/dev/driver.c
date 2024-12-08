#include "driver.h"
#include "dev/device.h"
#include "dev/driver.h"
#include "dev/loader.h"
#include "lightos/dev/shared.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "oss/obj.h"
#include "sync/mutex.h"
#include <libk/string.h>
#include <mem/heap.h>

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

        ret->m_obj = create_oss_obj(handle->m_name);

        /* Make sure our object knows about us */
        oss_obj_register_child(ret->m_obj, ret, OSS_OBJ_TYPE_DRIVER, destroy_driver);
    } else {
        ret->m_flags |= DRV_DEFERRED_HNDL;
    }

    /*
     * NOTE: drivers use the kernels page dir, which is why we can pass NULL
     * This might be SUPER DUPER dumb but like idc and it's extra work to isolate
     * drivers from the kernel, so yea fuckoff
     */
    ret->m_resources = create_resource_bundle(NULL);

    ret->m_max_n_dev = DRIVER_MAX_DEV_COUNT;
    // ret->m_dev_list = create_vector(ret->m_max_n_dev, sizeof(device_t*), VEC_FLAG_NO_DUPLICATES);
    ret->m_dev_list = init_list();

    return ret;
}

kerror_t driver_emplace_handle(driver_t* driver, aniva_driver_t* handle)
{
    if (driver->m_handle || (driver->m_flags & DRV_DEFERRED_HNDL) != DRV_DEFERRED_HNDL)
        return -1;

    ASSERT_MSG(driver->m_obj == nullptr || strcmp(driver->m_obj->name, handle->m_name) == 0, "Tried to emplace a handle on a driver which already has an object");

    /* Mark the driver as non-deferred */
    driver->m_flags &= ~DRV_DEFERRED_HNDL;
    driver->m_flags |= DRV_HAS_HANDLE;

    /* Emplace the handle and its data */
    driver->m_handle = handle;
    driver->m_check_version = handle->m_version;
    driver->m_url = get_driver_url(handle);

    if (!driver->m_obj) {
        driver->m_obj = create_oss_obj(handle->m_name);

        /* Make sure our object knows about us */
        oss_obj_register_child(driver->m_obj, driver, OSS_OBJ_TYPE_DRIVER, destroy_driver);
    }
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
    /*
     * An attached driver can be destroyed in two ways:
     * 1) Using this function directly. In this case we need to check if we are still attached to OSS and if we are
     * that means we need to destroy ourselves through that mechanism
     * 2) Through OSS. When we're attached to OSS, we have our own oss object that is attached to an oss node. When
     * the driver was created we registered our destruction method there, which means that all driver memory
     * is owned by the oss_object
     */
    if (driver->m_obj && driver->m_obj->priv == driver) {
        /* Calls destroy_driver again with ->priv cleared */
        destroy_oss_obj(driver->m_obj);
        return;
    }

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

    /* Destroy the resources */
    destroy_resource_bundle(driver->m_resources);

    /* Free strings and stuff */
    kfree((void*)driver->m_url);
    if (driver->m_image_path)
        kfree((void*)driver->m_image_path);
    free_ddriver(driver);
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

    /* NOTE: This is suddenly a bool :clown: */
    if (!sysvar_get_str_value(drv_root, &relative_path))
        return nullptr;

    abs_path_len = strlen(relative_path) + strlen(dep->location) + 1;
    abs_path = kmalloc(abs_path_len);

    /* Construct the full path */
    memset(abs_path, 0, abs_path_len);
    concat((char*)relative_path, (char*)dep->location, abs_path);

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
