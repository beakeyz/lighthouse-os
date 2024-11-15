#include <lightos/proc/cmdline.h>

#include <errno.h>
#include <time.h>

static CMDLINE sysconf_cmdline;

static const char sysconf_target[128] = { NULL };
static const char sysconf_output[128] = { NULL };
static const char sysconf_str_value[236] = { NULL };
static uint64_t sysconf_int_value = NULL;
static bool sysconf_should_write = false;
static bool sysconf_should_read = false;

/*!
 * @brief: Entrypoint for the system config program
 *
 * sysconf is an admin app that enables cmd-line system configuration.
 * The program is built up of two components:
 *
 * sysconf         (binary executable)
 * sysconf.lib     (Backend sysconf library)
 *
 * This code is for the binary executable, which itself uses the sysconf.lib library.
 *
 * With this, you can:
 *  - Edit sysvars: Change their current value, but don't save them permanently
 *  - Read sysvars: Read out the value of certain sysvars
 *  - Compute sysvars: Perform computations (??? Concept)
 *  - Save sysvars: Save sysvars to files
 *  - Load sysvars: Load sysvars from files
 */
int main()
{
    /* Grab the sysconf commandline */
    if (cmdline_get(&sysconf_cmdline))
        return -EIO;

    /* Parse the retrieved commandline */
    return 0;
}
