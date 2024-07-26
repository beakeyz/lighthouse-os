#include <lightos/lib/lightos.h>

/*!
 * @brief: Entry function for the lightUI library
 *
 * This library is an extention uppon the pretty low-level libgfx library, which handles
 * communication with the window management driver. This driver aims to standardise UI
 * and graphics development, window layout, user sided window management, ect.
 *
 * When apps create windows through this library, they will gain:
 *  - An indicator for when their window is focussed
 *  - A functional window overlay (close, minimize, maximize buttons, resize capabilities, ect.)
 *  - A widget library to create standardized interfaces
 *
 */
LIGHTENTRY int lib_entry(void)
{
    return 0;
}
