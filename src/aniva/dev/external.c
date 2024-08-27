#include "external.h"
#include "dev/driver.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
#include "sync/mutex.h"

static mutex_t* _ext_drv_lock = NULL;
static list_t* _ext_drv_list = NULL;

static inline void _register_ext_driver(extern_driver_t* drv)
{
    ASSERT_MSG(_ext_drv_lock && _ext_drv_list, "Tried to register an external driver without the appropriate structures");

    mutex_lock(_ext_drv_lock);

    list_append(_ext_drv_list, drv);

    mutex_unlock(_ext_drv_lock);
}

static inline void _unregister_ext_driver(extern_driver_t* drv)
{
    ASSERT_MSG(_ext_drv_lock && _ext_drv_list, "Tried to unregister an external driver without the appropriate structures");

    mutex_lock(_ext_drv_lock);

    list_remove_ex(_ext_drv_list, drv);

    mutex_unlock(_ext_drv_lock);
}

extern_driver_t* create_external_driver(uint32_t flags)
{
    extern_driver_t* drv;

    drv = kzalloc(sizeof(extern_driver_t));

    if (!drv)
        return nullptr;

    memset(drv, 0, sizeof(extern_driver_t));

    drv->m_flags = flags;
    drv->m_exp_symmap = create_hashmap(512, NULL);

    if ((flags & EX_DRV_OLDMANIFEST) != EX_DRV_OLDMANIFEST) {
        drv->m_driver = create_driver(nullptr);
        drv->m_driver->m_flags |= (DRV_IS_EXTERNAL);
    }

    /* Add a mofo */
    _register_ext_driver(drv);

    return drv;
}

/*
 * TODO: move this resource clear routine to the resource context subsystem
 *
 * NOTE: caller should have the drivers mutex held
 */
void destroy_external_driver(extern_driver_t* ext_driver)
{
    driver_t* driver;

    /* Bummer */
    if (!ext_driver)
        return;

    /* Unregister the driver */
    _unregister_ext_driver(ext_driver);

    driver = ext_driver->m_driver;

    /*
     * We need to let the driver know that it does not have a driver anymore xD
     */
    if (driver) {
        driver->m_handle = nullptr;
        driver->m_external = nullptr;

        driver->m_flags &= ~DRV_HAS_HANDLE;
        driver->m_flags |= DRV_DEFERRED_HNDL;

        /* Make sure we also deallocate our load base, just in case */
        if (ext_driver->m_load_size)
            (__kmem_dealloc(nullptr, driver->m_resources, ext_driver->m_load_base, ext_driver->m_load_size));
    }

    /* Some driver may not even have an exported symmap */
    if (ext_driver->m_exp_symmap)
        destroy_hashmap(ext_driver->m_exp_symmap);

    kzfree(driver, sizeof(extern_driver_t));
}

void set_exported_drvsym(extern_driver_t* driver, char* name, vaddr_t addr)
{
    if (!driver->m_exp_symmap)
        driver->m_exp_symmap = create_hashmap(512, NULL);

    hashmap_put(driver->m_exp_symmap, name, (void*)addr);
}

/*!
 * @brief: Get a symbol exported by a driver
 */
vaddr_t get_exported_drvsym(char* name)
{
    vaddr_t ret;

    FOREACH(i, _ext_drv_list)
    {
        extern_driver_t* drv = i->data;

        if (!drv->m_exp_symmap)
            continue;

        ret = (vaddr_t)hashmap_get(drv->m_exp_symmap, name);

        if (ret)
            break;
    }

    return ret;
}

/*!
 * @brief: Setup structures needed for external drivers
 */
void init_external_drivers()
{
    _ext_drv_lock = create_mutex(NULL);
    _ext_drv_list = init_list();
}
