#ifndef __LIGHTOS_MODULES_H__
#define __LIGHTOS_MODULES_H__

#include <std/types.h>

typedef struct modules_version {
    unsigned char major;
    unsigned char minor;
    unsigned char bump;
    unsigned char patch;
} modules_version_t;

#define MODULES_VERSION(maj, min, bump, patch) \
    (modules_version_t) { (maj), (min), (bump), (patch) }

extern modules_version_t get_modules_version();

/*
 * Context for platform-dependent functions
 *
 * Contains functions that provide functionality that needs to
 * know platform specific stuff. Since the modules
 */
typedef struct modules_ctx {
    void* (*f_alloc)(size_t size);
    void (*f_dealloc)(void* addr, size_t size);
} modules_ctx_t;

extern void init_modules(modules_ctx_t* ctx);

extern modules_ctx_t* g_modules_ctx;
extern modules_version_t g_modules_version;

#endif // !__LIGHTOS_MODULES_H__
