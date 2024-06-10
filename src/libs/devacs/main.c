
#include "lightos/lib/lightos.h"

extern int init_devices();

/*!
 * @brief: Entrypoint of the library
 *
 * Called when the library gets loaded by or for a process
 * Put any initialisation code here
 */
LIGHTENTRY int devacs_init()
{
    int error;

    error = init_devices();

    return error;
}
