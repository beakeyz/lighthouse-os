#include <lightos/lib/lightos.h>

/*!
 * @brief: sysconf.lib entry point
 *
 * Called when the library gets loaded by the system. Here, we need to check a few things
 * and get caches up and running.
 * The purpose of this library is to provide userspace with a consistant interface to perform
 * system configuration. Whether this entails changing the configuration of a single app, or
 * changing some lower-level driver configuration, the opperation should remain the same simple,
 * trivial and stable task. The system should be robust enough to be able to sustain changes in
 * it's configuration, since we have kernel events for changes in the sysvar space.
 *
 * TODO: Create a driver library for exposing a sysvar configuration layer.
 */
LIGHTENTRY int lib_entry()
{
    return 0;
}
