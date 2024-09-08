#ifndef __ANIVA_PROC_HANDLE_DRIVER_H__
#define __ANIVA_PROC_HANDLE_DRIVER_H__

#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "proc/handle.h"
#include <libk/stddef.h>

struct driver;

typedef struct khandle_driver {
    const char* name;

    /* The type this driver covers */
    HANDLE_TYPE handle_type;

    int (*f_open)(struct khandle_driver* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);
    int (*f_open_relative)(struct khandle_driver* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);

    int (*f_read)(struct khandle_driver* driver, khandle_t* hndl, void* buffer, size_t bsize);
    int (*f_write)(struct khandle_driver* driver, khandle_t* hndl, void* buffer, size_t bsize);
} khandle_driver_t;

kerror_t khandle_driver_register(struct driver* parent, khandle_driver_t* driver);
kerror_t khandle_driver_remove(khandle_driver_t* driver);
kerror_t khandle_driver_remove_ex(HANDLE_TYPE type);

kerror_t khandle_driver_find(HANDLE_TYPE type, khandle_driver_t** pDriver);

/* Wrappers for the khandle driver default functions */
kerror_t khandle_driver_open(khandle_driver_t* driver, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);
kerror_t khandle_driver_open_relative(khandle_driver_t* driver, khandle_t* rel_hndl, const char* path, u32 flags, enum HNDL_MODE mode, khandle_t* bHandle);
kerror_t khandle_driver_read(khandle_driver_t* driver, khandle_t* hndl, void* buffer, size_t bsize);
kerror_t khandle_driver_write(khandle_driver_t* driver, khandle_t* hndl, void* buffer, size_t bsize);

extern void init_file_khandle_driver();
extern void init_dir_khandle_driver();

void init_khandle_drivers();

#endif // !__ANIVA_PROC_HANDLE_DRIVER_H__
