#include "lsdk.h"

/*!
 * Static variable that is built into every lsdk binary. Used to distinguish
 * different system components using these shared lsdk.
 *
 * For example the bootloader can use this to find out if the kernel it's trying to load
 * supports certain functions
 */
lsdk_version_t g_lsdk_version = LSDK_VERSION(0, 1, 0, 0);
lsdk_ctx_t* g_lsdk_ctx;

/*!
 * @brief: Wrapper for the lsdk version
 */
lsdk_version_t get_lsdk_version()
{
    return g_lsdk_version;
}

void init_lsdk(lsdk_ctx_t* ctx)
{
    g_lsdk_ctx = ctx;
}
