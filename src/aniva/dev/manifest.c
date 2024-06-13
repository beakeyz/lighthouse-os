#include "manifest.h"
#include "dev/core.h"
#include "dev/device.h"
#include "dev/driver.h"
#include "dev/loader.h"
#include "libk/data/vector.h"
#include "libk/flow/error.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "sync/mutex.h"
#include "system/resource.h"
#include <libk/string.h>
#include <stdint.h>

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

bool driver_manifest_write(aniva_driver_t* driver, int (*write_fn)())
{
    dev_url_t path;
    drv_manifest_t* manifest;

    if (!driver || !write_fn)
        return false;

    path = get_driver_url(driver);

    if (!path)
        return false;

    manifest = get_driver(path);

    if (!manifest)
        return false;

    kfree((void*)path);

    mutex_lock(manifest->m_lock);

    manifest->m_ops.f_write = write_fn;

    mutex_unlock(manifest->m_lock);

    return true;
}

bool driver_manifest_read(aniva_driver_t* driver, int (*read_fn)())
{
    dev_url_t path;
    drv_manifest_t* manifest;

    if (!driver || !read_fn)
        return false;

    path = get_driver_url(driver);

    if (!path)
        return false;

    manifest = get_driver(path);

    kfree((void*)path);

    if (!manifest)
        return false;

    mutex_lock(manifest->m_lock);

    manifest->m_ops.f_read = read_fn;

    mutex_unlock(manifest->m_lock);

    return true;
}

drv_manifest_t* create_drv_manifest(aniva_driver_t* handle)
{
    drv_manifest_t* ret;

    ret = allocate_dmanifest();

    ASSERT(ret);

    memset(ret, 0, sizeof(*ret));

    ret->m_lock = create_mutex(NULL);
    ret->m_handle = handle;

    ret->m_flags = NULL;
    ret->m_dep_count = NULL;
    ret->m_dep_list = NULL;

    /* Reset the manifest opperations */
    memset(&ret->m_ops, 0, sizeof(ret->m_ops));

    if (handle) {
        ret->m_check_version = handle->m_version;
        // TODO: concat
        ret->m_url = get_driver_url(handle);

        ret->m_obj = create_oss_obj(handle->m_name);

        /* Make sure our object knows about us */
        oss_obj_register_child(ret->m_obj, ret, OSS_OBJ_TYPE_DRIVER, destroy_drv_manifest);
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
    ret->m_dev_list = create_vector(ret->m_max_n_dev, sizeof(device_t*), VEC_FLAG_NO_DUPLICATES);

    return ret;
}

kerror_t manifest_emplace_handle(drv_manifest_t* manifest, aniva_driver_t* handle)
{
    if (manifest->m_handle || (manifest->m_flags & DRV_DEFERRED_HNDL) != DRV_DEFERRED_HNDL)
        return -1;

    ASSERT_MSG(manifest->m_obj == nullptr || strcmp(manifest->m_obj->name, handle->m_name) == 0, "Tried to emplace a handle on a manifest which already has an object");

    /* Mark the manifest as non-deferred */
    manifest->m_flags &= ~DRV_DEFERRED_HNDL;
    manifest->m_flags |= DRV_HAS_HANDLE;

    /* Emplace the handle and its data */
    manifest->m_handle = handle;
    manifest->m_check_version = handle->m_version;
    manifest->m_url = get_driver_url(handle);

    if (!manifest->m_obj) {
        manifest->m_obj = create_oss_obj(handle->m_name);

        /* Make sure our object knows about us */
        oss_obj_register_child(manifest->m_obj, manifest, OSS_OBJ_TYPE_DRIVER, destroy_drv_manifest);
    }
    return (0);
}

/*!
 * @brief Destroy a manifest and deallocate it's resources
 *
 * This assumes that the underlying driver has already been taken care of
 *
 * NOTE: does not destroy the devices on this manifest. Caller needs to make sure this is
 * done. The driver behind this manifest needs to implement functions that handle the propper
 * destruction of the devices, since it is the one who owns this memory
 */
void destroy_drv_manifest(drv_manifest_t* manifest)
{
    /*
     * An attached driver can be destroyed in two ways:
     * 1) Using this function directly. In this case we need to check if we are still attached to OSS and if we are
     * that means we need to destroy ourselves through that mechanism
     * 2) Through OSS. When we're attached to OSS, we have our own oss object that is attached to an oss node. When
     * the manifest was created we registered our destruction method there, which means that all drv_manifest memory
     * is owned by the oss_object
     */
    if (manifest->m_obj && manifest->m_obj->priv == manifest) {
        /* Calls destroy_drv_manifest again with ->priv cleared */
        destroy_oss_obj(manifest->m_obj);
        return;
    }

    destroy_mutex(manifest->m_lock);

    /* A manifest might exist without any dependencies */
    if (manifest->m_dep_list)
        destroy_vector(manifest->m_dep_list);

    /*
     * Murder the vector for devices
     * TODO/FIXME: What do we do with attached devices when
     * a driver gets destroyed?
     */
    destroy_vector(manifest->m_dev_list);

    /* Destroy the resources */
    destroy_resource_bundle(manifest->m_resources);

    /* Free strings and stuff */
    kfree((void*)manifest->m_url);
    if (manifest->m_driver_file_path)
        kfree((void*)manifest->m_driver_file_path);
    free_dmanifest(manifest);
}

static drv_manifest_t* _get_dependency_from_path(drv_dependency_t* dep)
{
    kerror_t error;
    char* abs_path;
    size_t abs_path_len;
    const char* relative_path;
    sysvar_t* drv_root;
    drv_manifest_t* ret;

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
 * @brief: 'serialize' the given driver paths into the manifest
 *
 * This ONLY gathers the dependencies. It does not do any driver loading
 */
int manifest_gather_dependencies(drv_manifest_t* manifest)
{
    manifest_dependency_t man_dep;
    aniva_driver_t* handle = manifest->m_handle;

    /* We should not have any dependencies on the manifest at this point */
    ASSERT(!manifest->m_dep_count);
    ASSERT(!manifest->m_dep_list);

    if (!handle->m_deps || !handle->m_deps->location)
        return KERR_INVAL;

    /* Count the dependencies */
    while (handle->m_deps[manifest->m_dep_count].location)
        manifest->m_dep_count++;

    if (!manifest->m_dep_count)
        return KERR_INVAL;

    manifest->m_dep_list = create_vector(manifest->m_dep_count, sizeof(manifest_dependency_t), NULL);

    /*
     * We kinda trust too much in the fact that m_dep_count is
     * correct...
     */
    for (uintptr_t i = 0; i < manifest->m_dep_count; i++) {

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
        vector_add(manifest->m_dep_list, &man_dep, NULL);
    }

    return 0;
}

int manifest_add_dev(struct drv_manifest* driver, device_t* device)
{
    kerror_t error;

    if (!driver || !device)
        return -1;

    mutex_lock(driver->m_lock);

    error = vector_add(driver->m_dev_list, &device, NULL);

    mutex_unlock(driver->m_lock);

    return (error) ? -KERR_NOMEM : 0;
}

int manifest_remove_dev(struct drv_manifest* driver, device_t* device)
{
    int error = -KERR_NOT_FOUND;
    uint32_t idx = 0;

    if (!driver || !device)
        return -1;

    mutex_lock(driver->m_lock);

    /* Try to get the devices index */
    if (!manifest_has_dev(driver, device, &idx))
        goto unlock_and_exit;

    /* Try to remove from the vector */
    if ((vector_remove(driver->m_dev_list, idx)))
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
bool manifest_has_dev(struct drv_manifest* driver, struct device* device, uint32_t* p_idx)
{
    kerror_t error;

    mutex_lock(driver->m_lock);

    error = vector_indexof(driver->m_dev_list, device, p_idx);

    mutex_unlock(driver->m_lock);

    /* If we where able to find an index, the driver has this device */
    return (error == 0);
}
