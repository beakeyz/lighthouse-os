/*!
 * @brief: Perform runtime system initialisation from the user-side
 *
 * This process is sent into userland by the kernel to do things there on its behalf xD
 * The kernel has already done some basic device detection and driver matching, but it might
 * have fucked up at some fronts. This process further loads some device/utility drivers and
 * does some more scanning.
 *
 * What should the init process do?
 *  - Verify system integrity before bootstrapping further
 *  - Find various config files and load the ones it needs
 *     * Load and read some sysvar files which may contain driver lists we need to load \o/
 *  - Try to configure the kernel trough the essensial configs
 *  - Find the vector of services to run like, in no particular order:
 *      -- Display manager
 *      -- Sessions
 *      -- Network services
 *      -- Peripherals
 *      -- Audio
 *  - Find out if we need to install the system on a permanent storage device and
 *    if so, run the installer (which creates the partition, installs the fs and the system)
 *  - Find the vector of further bootstrap applications to run and run them
 */
#include "lightos/api/handle.h"

int main(HANDLE self)
{
    return -69;
}
