#ifndef __ANIVA_DRIVER__
#define __ANIVA_DRIVER__

#include "core.h"
#include <libk/stddef.h>

struct oss_obj;
struct device;
struct dgroup;
struct drv_manifest;

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

#define DRV_DEP(_type, _flags, _location)                            \
    {                                                                \
        .type = (_type), .flags = (_flags), .location = (_location), \
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
    dev_type_t m_type;

    /* TODO: should we pass the manifest to the f_init function? */
    int (*f_init)(struct drv_manifest* this);
    int (*f_exit)(void);

    /* TODO: should f_msg get passed the drv_manifest, instead of the raw driver? */
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

#define driver_is_deferred(manifest) ((manifest->m_flags & DRV_DEFERRED) == DRV_DEFERRED)

bool driver_is_ready(struct drv_manifest* manifest);
bool driver_is_busy(struct drv_manifest* manifest);

int drv_read(struct drv_manifest* manifest, void* buffer, size_t* buffer_size, uintptr_t offset);
int drv_write(struct drv_manifest* manifest, void* buffer, size_t* buffer_size, uintptr_t offset);

int driver_takeover_device(struct drv_manifest* manifest, struct device* device, const char* newname, struct dgroup* newgroup, void* dev_priv);

/*
 * Create a thread for this driver in the kernel process
 * and run the entry
 */
kerror_t bootstrap_driver(struct drv_manifest* manifest);

#endif //__ANIVA_DRIVER__
