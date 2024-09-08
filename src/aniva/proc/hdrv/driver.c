#include "driver.h"
#include "dev/driver.h"
#include "libk/flow/error.h"
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

void init_khandle_drivers()
{
    /* First, clear the khandle driver registry */
    memset(__khandle_drivers, 0, sizeof(__khandle_drivers));

    /* Second, make sure our mutex is initialized */
    __khandle_driver_lock = create_mutex(NULL);
}
