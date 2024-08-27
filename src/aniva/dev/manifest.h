#ifndef __ANIVA_DRV_MANIFEST__
#define __ANIVA_DRV_MANIFEST__

#include "dev/core.h"
#include "dev/device.h"
#include "dev/driver.h"
#include "libk/data/linkedlist.h"
#include "libk/data/vector.h"
#include "mem/zalloc/zalloc.h"
#include "sync/mutex.h"
#include "system/resource.h"
#include <libk/data/hashmap.h>

struct proc;
struct oss_obj;
struct drv_manifest;
struct extern_driver;

/*
 * We let drivers implement a few functions that are mostly meant to
 * simulate the file opperations like read, write, seek, ect. that some
 * linux / unix drivers would implement.
 *
 * TODO: Migrate to control codes lol
 */
typedef struct driver_ops {
    int (*f_write)(aniva_driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
    int (*f_read)(aniva_driver_t* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
} driver_ops_t;

typedef struct manifest_dependency {
    drv_dependency_t dep;
    union {
        struct drv_manifest* drv;
        struct proc* proc;
    } obj;
} manifest_dependency_t;

/*
 * Bundle that tracks most of the resource useage for a particular driver
 *
 * Meant to catch any leaking memory when a driver is destroyed. The driver API for
 * allocating with this struct in mind are located in src/dev/core.h (core.c)
 *
 * TODO: Implement
 */
typedef struct driver_resources {
    /* Resources for mapped and kmem_alloced regions */
    kresource_bundle_t* raw_resources;
    /* Allocator on wich common objects will be allocated */
    zone_allocator_t* zallocator;
    /* List that keeps track of the syscalls that have been allocated by this driver */
    list_t* syscall_list;
    /* List that keeps track of the non-persistent oss_objs created by this driver */
    list_t* oss_obj_list;

    size_t leaked_bytes;
    size_t total_allocated_bytes;
} driver_resources_t;

/*
 * Weird driver thing
 *
 * TODO: Clean up this mess and merge driver.c/h and manifest.c/h
 */
typedef struct drv_manifest {
    aniva_driver_t* m_handle;

    uint32_t m_dep_count;
    uint32_t m_max_n_dev;

    vector_t* m_dep_list;
    vector_t* m_dev_list;

    uint32_t m_flags;
    driver_version_t m_check_version;

    /* Resources that this driver has claimed */
    kresource_bundle_t* m_resources;
    mutex_t* m_lock;

    /* Url of the installed driver */
    dev_url_t m_url;

    struct oss_obj* m_obj;
    /* Any data that's specific to the kind of driver this is */
    // void* m_private;

    /* Path to the binary of the driver, only on external drivers */
    const char* m_driver_file_path;
    struct extern_driver* m_external;

    driver_ops_t m_ops;
} drv_manifest_t;

drv_manifest_t* create_drv_manifest(aniva_driver_t* handle);
void destroy_drv_manifest(drv_manifest_t* manifest);

int manifest_gather_dependencies(drv_manifest_t* manifest);

bool is_manifest_valid(drv_manifest_t* manifest);

kerror_t manifest_emplace_handle(drv_manifest_t* manifest, aniva_driver_t* handle);

bool driver_manifest_write(struct aniva_driver* manifest, int (*write_fn)());
bool driver_manifest_read(struct aniva_driver* manifest, int (*read_fn)());

int manifest_add_dev(struct drv_manifest* driver, struct device* device);
int manifest_remove_dev(struct drv_manifest* driver, struct device* device);
bool manifest_has_dev(struct drv_manifest* driver, struct device* device, uint32_t* p_idx);

static inline bool manifest_is_active(drv_manifest_t* manifest)
{
    return (manifest->m_flags & DRV_ACTIVE) == DRV_ACTIVE;
}

#endif // !__ANIVA_drv_manifest__
