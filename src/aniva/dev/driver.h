#ifndef __ANIVA_DRIVER__
#define __ANIVA_DRIVER__

#include "core.h"
#include "libk/data/linkedlist.h"
#include "libk/data/vector.h"
#include "mem/zalloc/zalloc.h"
#include "system/resource.h"
#include <libk/stddef.h>

struct oss_obj;
struct device;
struct dgroup;
struct driver;

/*
 * Every type of driver has a version
 */
typedef union driver_version {
    struct {
        uint8_t maj;
        uint8_t min;
        uint16_t bump;
    };
    uint32_t version;
} driver_version_t;

/*
 * Different types of dependencies
 */
enum DRV_DEPTYPE {
    DRV_DEPTYPE_URL,
    DRV_DEPTYPE_PATH,
    DRV_DEPTYPE_PROC,
};

#define DRVDEP_FLAG_RELPATH 0x00000001
#define DRVDEP_FLAG_OPTIONAL 0x00000002

/*
 * A single driver dependency entry
 *
 * These are provided by the driver and injected through the aniva_driver struct
 */
typedef struct drv_dependency {
    enum DRV_DEPTYPE type;
    uint32_t flags;
    const char* location;
} drv_dependency_t;

#define DRV_DEP(_type, _flags, _location) \
    {                                     \
        .type = (_type),                  \
        .flags = (_flags),                \
        .location = (_location),          \
    }
#define DRV_DEP_END      \
    {                    \
        NULL, NULL, NULL \
    }

static inline bool drv_dep_is_driver(drv_dependency_t* dep)
{
    return (
        dep->type == DRV_DEPTYPE_PATH || dep->type == DRV_DEPTYPE_URL);
}

static inline bool drv_dep_is_optional(drv_dependency_t* dep)
{
    return ((dep->flags & DRVDEP_FLAG_OPTIONAL) == DRVDEP_FLAG_OPTIONAL);
}

/*
 * Main struct to define the outline of a system driver
 *
 */
typedef struct aniva_driver {
    const char m_name[MAX_DRIVER_NAME_LENGTH];
    const char m_descriptor[MAX_DRIVER_DESCRIPTOR_LENGTH];

    driver_version_t m_version;
    enum DRIVER_TYPE m_type;

    /* TODO: should we pass the driver to the f_init function? */
    int (*f_init)(struct driver* this);
    int (*f_exit)(void);

    /* TODO: should f_msg get passed the driver, instead of the raw driver? */
    uintptr_t (*f_msg)(struct aniva_driver* driver, driver_control_code_t code, void* buffer, size_t size, void* out_buffer, size_t out_size);
    /*
     * Used to try and verify if this driver supports a device
     * NOTE: until now, this is unused...
     */
    int (*f_probe)(struct aniva_driver* driver, struct device* device);

    struct drv_dependency* m_deps;
} aniva_driver_t;

#define DRV_FS (0x00000001) /* Should be mounted inside the vfs */
#define DRV_NON_ESSENTIAL (0x00000002) /* Is this a system-driver or user/nonessential driver */
#define DRV_ACTIVE (0x00000004) /* Is this driver available for opperations */
#define DRV_ALLOW_DYNAMIC_LOADING (0x00000008) /* Allows the installed driver to be loaded when we need it */
#define DRV_HAS_HANDLE (0x00000010) /* This driver is tethered to a handle */
#define DRV_FAILED (0x00000020) /* Failiure to load =/ */
#define DRV_DEFERRED (0x00000040) /* This driver should be loaded at a later stage */
#define DRV_LIMIT_REACHED (0x00000080) /* This driver could not be verified, since the limit of its tipe has been reached */

#define DRV_WANT_PROC (0x00000100)
#define DRV_HAD_DEP (0x00000200)
#define DRV_SYSTEM (0x00000400)
#define DRV_ORPHAN (0x00000800)
#define DRV_IS_EXTERNAL (0x00001000)
#define DRV_HAS_MSG_FUNC (0x00002000)
#define DRV_DEFERRED_HNDL (0x00004000)

#define DRV_CORE (0x00008000) /* Is this driver for core functions? */
#define DRV_LOADED (0x00010000) /* Is this driver loaded? */

#define DRV_STAT_OK (0)
#define DRV_STAT_INVAL (-1)
#define DRV_STAT_NOMAN (-2)
#define DRV_STAT_BUSY (-3)
#define DRV_STAT_FULL (-4)
#define DRV_STAT_DUPLICATE (-5)
#define DRV_STAT_NULL (-6)
#define DRV_STAT_OUT_OF_RANGE (-7)
#define DRV_STAT_NOT_FOUND (-8)
#define DRV_STAT_NODATA (-9)

#define driver_is_deferred(driver) ((driver->m_flags & DRV_DEFERRED) == DRV_DEFERRED)

bool driver_is_ready(struct driver* driver);
bool driver_is_busy(struct driver* driver);

int drv_read(struct driver* driver, void* buffer, size_t* buffer_size, uintptr_t offset);
int drv_write(struct driver* driver, void* buffer, size_t* buffer_size, uintptr_t offset);

int driver_takeover_device(struct driver* driver, struct device* device, const char* newname, struct dgroup* newgroup, void* dev_priv);

/*
 * Create a thread for this driver in the kernel process
 * and run the entry
 */
kerror_t bootstrap_driver(struct driver* driver);

typedef struct driver_dependency {
    drv_dependency_t dep;
    union {
        struct driver* drv;
        struct proc* proc;
    } obj;
} driver_dependency_t;

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
 * TODO: Clean up this mess and merge driver.c/h and driver.c/h
 */
typedef struct driver {
    aniva_driver_t* m_handle;

    uint32_t m_dep_count;
    uint32_t m_max_n_dev;

    vector_t* m_dep_list;
    list_t* m_dev_list;

    uint32_t m_flags;
    driver_version_t m_check_version;

    /* Resources that this driver has claimed */
    kresource_bundle_t* m_resources;
    mutex_t* m_lock;

    /* Url of the installed driver */
    dev_url_t m_url;

    /* Device object that provides a control interface for the driver, set to NULL, when
     * the driver is created */
    struct device* m_ctl_device;

    struct oss_obj* m_obj;
    /* Any data that's specific to the kind of driver this is */
    // void* m_private;

    /* Path to the binary of the driver, only on external drivers */
    const char* m_image_path;

    /* Loader information */
    vaddr_t load_base;
    u64 load_size;
    u64 ksym_count;
} driver_t;

driver_t* create_driver(aniva_driver_t* handle);
void destroy_driver(driver_t* driver);

int driver_gather_dependencies(driver_t* driver);

bool is_driver_valid(driver_t* driver);

kerror_t driver_emplace_handle(driver_t* driver, aniva_driver_t* handle);

int driver_add_dev(struct driver* driver, struct device* device);
int driver_remove_dev(struct driver* driver, struct device* device);
bool driver_has_dev(struct driver* driver, struct device* device, uint32_t* p_idx);

int driver_set_ctl_device(struct driver* driver, struct device* device);
int driver_destroy_ctl_device(struct driver* driver);

static inline bool driver_is_active(driver_t* driver)
{
    return (driver->m_flags & DRV_ACTIVE) == DRV_ACTIVE;
}

#endif //__ANIVA_DRIVER__
