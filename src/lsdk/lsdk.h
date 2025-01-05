#ifndef __LIGHTOS_lsdk_H__
#define __LIGHTOS_lsdk_H__

#include <lsdk/types.h>

typedef struct lsdk_version {
    unsigned char major;
    unsigned char minor;
    unsigned char bump;
    unsigned char patch;
} lsdk_version_t;

#define LSDK_VERSION(maj, min, bump, patch) \
    (lsdk_version_t) { (maj), (min), (bump), (patch) }

extern lsdk_version_t get_lsdk_version();

/*
 * Context for platform-dependent functions
 *
 * Contains functions that provide functionality that needs to
 * know platform specific stuff. Since the lsdk
 */
typedef struct lsdk_ctx {
    void* (*f_alloc)(size_t size);
    void (*f_dealloc)(void* addr, size_t size);
} lsdk_ctx_t;

extern void init_lsdk(lsdk_ctx_t* ctx);

extern void* lsdk_alloc(size_t size);
extern void lsdk_dealloc(void* addr, size_t size);

extern lsdk_ctx_t g_lsdk_ctx;
extern lsdk_version_t g_lsdk_version;

#endif // !__LIGHTOS_lsdk_H__
