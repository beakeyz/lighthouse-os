#include "lsdk.h"

/*!
 * Static variable that is built into every lsdk binary. Used to distinguish
 * different system components using these shared lsdk.
 *
 * For example the bootloader can use this to find out if the kernel it's trying to load
 * supports certain functions
 */
lsdk_version_t g_lsdk_version = LSDK_VERSION(0, 1, 0, 0);
lsdk_ctx_t g_lsdk_ctx;

/*!
 * @brief: Wrapper for the lsdk version
 */
lsdk_version_t get_lsdk_version()
{
    return g_lsdk_version;
}

void* lsdk_alloc(size_t size)
{
    if (!g_lsdk_ctx.f_alloc)
        return nullptr;

    return g_lsdk_ctx.f_alloc(size);
}

void lsdk_dealloc(void* addr, size_t size)
{
    if (!g_lsdk_ctx.f_dealloc)
        return;

    return g_lsdk_ctx.f_dealloc(addr, size);
}

void init_lsdk(lsdk_ctx_t* ctx)
{
    if (!ctx)
        return;

    /* Context buffer for this lsdk instance */
    g_lsdk_ctx = (lsdk_ctx_t) { 0 };

    /* Set the functions */
    g_lsdk_ctx.f_alloc = ctx->f_alloc;
    g_lsdk_ctx.f_dealloc = ctx->f_dealloc;
}
