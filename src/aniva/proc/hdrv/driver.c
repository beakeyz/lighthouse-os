#include "driver.h"
#include "dev/driver.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "logging/log.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "proc/handle.h"
#include "sync/mutex.h"

#include <libk/string.h>

/**
 * @brief: khandle driver registry entry
 *
 * Stores information about a khandle driver of a particular khandle type
 */
struct khandle_driver_reg {
    driver_t* pDriver;
    khandle_driver_t* pHandleDriver;
};

static struct khandle_driver_reg __khandle_drivers[N_HNDL_TYPES];
static mutex_t* __khandle_driver_lock;

extern khandle_driver_t* _khdrv_start[];
extern khandle_driver_t* _khdrv_end[];

#define FOREACH_KHNDL_DRV(i) for (struct khandle_driver** p_##i = _khdrv_start, *i = *(p_##i); p_##i < _khdrv_end; p_##i++, i = *(p_##i))

/**
 * @brief: Register a khandle driver
 *
 * Parent may be NULL, which indicates this driver is added and handled by the kernel itself
 */
kerror_t khandle_driver_register(driver_t* parent, khandle_driver_t* driver)
{
    struct khandle_driver_reg* slot;

    /* Check for invalid params */
    if (!driver || driver->handle_type >= N_HNDL_TYPES)
        return -KERR_INVAL;

    /* Grab the slot for this driver type */
    slot = &__khandle_drivers[driver->handle_type];

    /* Check if there is already a driver present */
    if (slot->pHandleDriver)
        return -KERR_DUPLICATE;

    mutex_lock(__khandle_driver_lock);

    /* Slot is free, put the driver there */
    slot->pDriver = parent;
    slot->pHandleDriver = driver;

    /* Call the driver init function if that's present */
    khandle_driver_init(driver);

    mutex_unlock(__khandle_driver_lock);

    return 0;
}

kerror_t khandle_driver_remove(khandle_driver_t* driver)
{
    if (!driver)
        return -KERR_INVAL;

    return khandle_driver_remove_ex(driver->handle_type);
}

kerror_t khandle_driver_remove_ex(HANDLE_TYPE type)
{
    struct khandle_driver_reg* slot;

    if (type >= N_HNDL_TYPES)
        return -KERR_RANGE;

    mutex_lock(__khandle_driver_lock);

    slot = &__khandle_drivers[type];

    /* Call the driver fini function if that's present */
    khandle_driver_fini(slot->pHandleDriver);

    /* Remove the driver from this slot */
    slot->pDriver = nullptr;
    slot->pHandleDriver = nullptr;

    mutex_unlock(__khandle_driver_lock);

    return 0;
}

/**
 * @brief: Find a khandle driver based on type
 */
kerror_t khandle_driver_find(HANDLE_TYPE type, khandle_driver_t** pDriver)
{
    khandle_driver_t* driver;

    if (!pDriver || type >= N_HNDL_TYPES)
        return -KERR_RANGE;

    /* Reset */
    *pDriver = NULL;

    mutex_lock(__khandle_driver_lock);

    /* Grab the target driver */
    driver = __khandle_drivers[type].pHandleDriver;

    mutex_unlock(__khandle_driver_lock);

    if (!driver)
        return -KERR_NOT_FOUND;

    /* Export the driver with the mutex unlocked */
    *pDriver = driver;

    return 0;
}

kerror_t khandle_driver_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    if (!driver || !driver->f_open)
        return -KERR_INVAL;

    if ((driver->function_flags & KHDRIVER_FUNC_OPEN) != KHDRIVER_FUNC_OPEN)
        return -KERR_NOT_FOUND;

    return driver->f_open(driver, path, flags, mode, bHandle);
}

kerror_t khandle_driver_open_relative(khandle_driver_t* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    if (!driver || !driver->f_open_relative)
        return -KERR_INVAL;

    if ((driver->function_flags & KHDRIVER_FUNC_OPEN_REL) != KHDRIVER_FUNC_OPEN_REL)
        return -KERR_NOT_FOUND;

    return driver->f_open_relative(driver, rel_hndl, path, flags, mode, bHandle);
}

kerror_t khandle_driver_read(khandle_driver_t* driver, khandle_t* hndl, void* buffer, size_t bsize)
{
    if (!driver || !driver->f_read)
        return -KERR_INVAL;

    if ((driver->function_flags & KHDRIVER_FUNC_READ) != KHDRIVER_FUNC_READ)
        return -KERR_NOT_FOUND;

    return driver->f_read(driver, hndl, buffer, bsize);
}

kerror_t khandle_driver_write(khandle_driver_t* driver, khandle_t* hndl, void* buffer, size_t bsize)
{
    if (!driver || !driver->f_write)
        return -KERR_INVAL;

    if ((driver->function_flags & KHDRIVER_FUNC_WRITE) != KHDRIVER_FUNC_WRITE)
        return -KERR_NOT_FOUND;

    return driver->f_write(driver, hndl, buffer, bsize);
}

kerror_t khandle_driver_ctl(khandle_driver_t* driver, khandle_t* hndl, enum DEVICE_CTLC ctl, u64 offset, void* buffer, size_t bsize)
{
    if (!driver || !driver->f_ctl)
        return -KERR_INVAL;

    if ((driver->function_flags & KHDRIVER_FUNC_CTL) != KHDRIVER_FUNC_CTL)
        return -KERR_NOT_FOUND;

    return driver->f_ctl(driver, hndl, ctl, offset, buffer, bsize);
}

/**
 * @brief: Generic open function for khandle drivers
 *
 * This is used by handle drivers that store their objects on the object storage system (or vfs if you like unix)
 */
kerror_t khandle_driver_generic_oss_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    return khandle_driver_generic_oss_open_from(driver, NULL, path, flags, mode, bHandle);
}

kerror_t khandle_driver_generic_oss_open_from(khandle_driver_t* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle)
{
    int error;
    void* handle_ref;
    oss_obj_t* out_obj;
    oss_node_t* rel_node = NULL;
    HANDLE_TYPE type = driver->handle_type;

    /* If there is a relative handle passed, use it to try and find a relative node */
    if (rel_hndl)
        rel_node = khandle_get_relative_node(rel_hndl);

    /* Find the raw object */
    error = oss_resolve_obj_rel(rel_node, path, &out_obj);

    /* Check to see if we might have fucked up */
    if (error)
        return error;

    /* Check it this object is of the type we want */
    if (oss_obj_type_to_handle_type(out_obj->type) != type) {
        oss_obj_close(out_obj);
        return -KERR_TYPE_MISMATCH;
    }

    switch (type) {
    case HNDL_TYPE_OSS_OBJ:
        handle_ref = out_obj;
        break;
    default:
        handle_ref = out_obj->priv;
        break;
    }

    /* Initialize the handle */
    init_khandle(bHandle, &type, handle_ref);

    /* Set the handle flags */
    khandle_set_flags(bHandle, flags);

    return 0;
}

void init_khandle_drivers()
{
    /* First, clear the khandle driver registry */
    memset(__khandle_drivers, 0, sizeof(__khandle_drivers));

    /* Second, make sure our mutex is initialized */
    __khandle_driver_lock = create_mutex(NULL);

    /* Register all internal khandle drivers */
    FOREACH_KHNDL_DRV(i)
    {
        KLOG_DBG("[khndl] Initializing khandle driver: %s\n", i->name);

        /* Register this driver */
        khandle_driver_register(NULL, i);
    }
}
