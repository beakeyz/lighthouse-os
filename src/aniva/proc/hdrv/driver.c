#include "driver.h"
#include "dev/driver.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "logging/log.h"
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

static struct khandle_driver_reg __khandle_drivers[NR_HNDL_TYPES];
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
    if (!driver || driver->handle_type >= NR_HNDL_TYPES)
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

    if (type >= NR_HNDL_TYPES)
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

    if (!pDriver || type >= NR_HNDL_TYPES)
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

kerror_t khandle_driver_close(khandle_driver_t* driver, khandle_t* handle)
{
    if (!driver || !driver->f_close)
        return -KERR_INVAL;

    if ((driver->function_flags & KHDRIVER_FUNC_CLOSE) != KHDRIVER_FUNC_CLOSE)
        return -KERR_NOT_FOUND;

    return driver->f_close(driver, handle);
}

kerror_t khandle_driver_read(khandle_driver_t* driver, khandle_t* hndl, u64 offset, void* buffer, size_t bsize)
{
    if (!driver || !driver->f_read)
        return -KERR_INVAL;

    if ((driver->function_flags & KHDRIVER_FUNC_READ) != KHDRIVER_FUNC_READ)
        return -KERR_NOT_FOUND;

    return driver->f_read(driver, hndl, offset, buffer, bsize);
}

kerror_t khandle_driver_write(khandle_driver_t* driver, khandle_t* hndl, u64 offset, void* buffer, size_t bsize)
{
    if (!driver || !driver->f_write)
        return -KERR_INVAL;

    if ((driver->function_flags & KHDRIVER_FUNC_WRITE) != KHDRIVER_FUNC_WRITE)
        return -KERR_NOT_FOUND;

    return driver->f_write(driver, hndl, offset, buffer, bsize);
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
