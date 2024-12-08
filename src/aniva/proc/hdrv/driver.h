#ifndef __ANIVA_PROC_HANDLE_DRIVER_H__
#define __ANIVA_PROC_HANDLE_DRIVER_H__

#include "lightos/dev/shared.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "proc/handle.h"
#include <libk/stddef.h>

struct driver;

#define KHDRIVER_FUNC_OPEN 0x00000001
#define KHDRIVER_FUNC_OPEN_REL 0x00000002
#define KHDRIVER_FUNC_CLOSE 0x00000004
#define KHDRIVER_FUNC_READ 0x00000008
#define KHDRIVER_FUNC_WRITE 0x00000010
#define KHDRIVER_FUNC_CTL 0x00000020
#define KHDRIVER_FUNC_DESTROY 0x00000040

/* Indicates that this driver implements all khandle I/O functions */
#define KHDRIVER_FUNC_ALL_IO 0x0000ffff

/* These are internal functions, but they should still be marked as present in the flags */
#define KHDRIVER_FUNC_INIT 0x00020000
#define KHDRIVER_FUNC_FINI 0x00040000
#define KHDRIVER_FUNCS_FINIT (KHDRIVER_FUNC_INIT | KHDRIVER_FUNC_FINI)

/* Indicates that this driver implements all khandle functions */
#define KHDRIVER_FUNC_ALL 0xffffffff

typedef struct khandle_driver {
    const char* name;

    /* The type this driver covers */
    HANDLE_TYPE handle_type;
    /* Tells us which functions this driver implements */
    u32 function_flags;

    /*
     * Init function. Called when the driver gets registered
     * This gets called with the khandle driver mutex taken
     */
    void (*f_init)(struct khandle_driver* driver);
    /*
     * Fini function. Called when the driver gets unregistered
     * This gets called with the khandle driver mutex taken
     */
    void (*f_fini)(struct khandle_driver* driver);

    int (*f_open)(struct khandle_driver* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);
    int (*f_open_relative)(struct khandle_driver* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);

    int (*f_close)(struct khandle_driver* driver, khandle_t* handle);

    int (*f_read)(struct khandle_driver* driver, khandle_t* hndl, void* buffer, size_t bsize);
    int (*f_write)(struct khandle_driver* driver, khandle_t* hndl, void* buffer, size_t bsize);
    int (*f_ctl)(struct khandle_driver* driver, khandle_t* hndl, enum DEVICE_CTLC ctl, u64 offset, void* buffer, size_t bsize);
    int (*f_destroy)(struct khandle_driver* driver, khandle_t* handle);
} khandle_driver_t;

static inline bool khandle_driver_has_init(khandle_driver_t* driver)
{
    return (driver->f_init && (driver->function_flags & KHDRIVER_FUNC_INIT) == KHDRIVER_FUNC_INIT);
}

static inline bool khandle_driver_has_fini(khandle_driver_t* driver)
{
    return (driver->f_fini && (driver->function_flags & KHDRIVER_FUNC_FINI) == KHDRIVER_FUNC_FINI);
}

/**
 * @brief: Calls the khdriver init function, if it's present
 */
static inline void khandle_driver_init(khandle_driver_t* driver)
{
    if (!khandle_driver_has_init(driver))
        return;

    driver->f_init(driver);
}

/**
 * @brief: Calls the khdriver fini function, if it's present
 */
static inline void khandle_driver_fini(khandle_driver_t* driver)
{
    if (!khandle_driver_has_fini(driver))
        return;

    driver->f_fini(driver);
}

/* Macro that puts a khandle driver pointer into the .khndl_drvs section */
#define KHNDL_DRVS_SECTION_NAME ".khdrv"
#define EXPORT_KHANDLE_DRIVER(driver)     \
    USED SECTION(KHNDL_DRVS_SECTION_NAME) \
    khandle_driver_t* exported_##driver = &(driver)

kerror_t khandle_driver_register(struct driver* parent, khandle_driver_t* driver);
kerror_t khandle_driver_remove(khandle_driver_t* driver);
kerror_t khandle_driver_remove_ex(HANDLE_TYPE type);

kerror_t khandle_driver_find(HANDLE_TYPE type, khandle_driver_t** pDriver);

/* Wrappers for the khandle driver default functions */
kerror_t khandle_driver_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);
kerror_t khandle_driver_open_relative(khandle_driver_t* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);
kerror_t khandle_driver_close(khandle_driver_t* driver, khandle_t* handle);
kerror_t khandle_driver_read(khandle_driver_t* driver, khandle_t* hndl, void* buffer, size_t bsize);
kerror_t khandle_driver_write(khandle_driver_t* driver, khandle_t* hndl, void* buffer, size_t bsize);
kerror_t khandle_driver_ctl(khandle_driver_t* driver, khandle_t* hndl, enum DEVICE_CTLC ctl, u64 offset, void* buffer, size_t bsize);

kerror_t khandle_driver_generic_oss_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);
kerror_t khandle_driver_generic_oss_open_from(khandle_driver_t* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);

void init_khandle_drivers();

#endif // !__ANIVA_PROC_HANDLE_DRIVER_H__
