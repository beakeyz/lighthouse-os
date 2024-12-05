#include "modules.h"

/*!
 * Static variable that is built into every modules binary. Used to distinguish
 * different system components using these shared modules.
 *
 * For example the bootloader can use this to find out if the kernel it's trying to load
 * supports certain functions
 */
modules_version_t g_modules_version = MODULES_VERSION(0, 1, 0, 0);

/*!
 * @brief: Wrapper for the modules version
 */
extern modules_version_t get_modules_version()
{
    return g_modules_version;
}
