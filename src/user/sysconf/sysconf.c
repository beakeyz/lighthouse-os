#include <lightos/proc/cmdline.h>
#include <params/params.h>

#include <errno.h>

static CMDLINE sysconf_cmdline;

static const char sysconf_target[128] = { NULL };
static const char sysconf_output[128] = { NULL };
static const char sysconf_str_value[236] = { NULL };
static uint64_t sysconf_int_value = NULL;
static bool sysconf_should_write = false;
static bool sysconf_should_read = false;

static cmd_param_t sysconf_params[] = {
    CMD_PARAM_AUTO(sysconf_target, CMD_PARAM_TYPE_STRING, 't', "Sets the target sysvar or target sysvar path (based on command type)"),
    CMD_PARAM_AUTO(sysconf_output, CMD_PARAM_TYPE_STRING, 'o', "Sets the output path"),
    CMD_PARAM_AUTO(sysconf_str_value, CMD_PARAM_TYPE_STRING, 's', "Sets the value of the target sysvar (If it's of the \'string\' type)"),
    CMD_PARAM_AUTO(sysconf_int_value, CMD_PARAM_TYPE_NUM, 'n', "Sets the value of the target sysvar (If it's of the \'number\' type)"),
    CMD_PARAM_AUTO(sysconf_should_write, CMD_PARAM_TYPE_BOOL, 'w', "Writes the target to the output"),
    CMD_PARAM_AUTO(sysconf_should_read, CMD_PARAM_TYPE_BOOL, 'r', "Reads the target to the output"),
};
static uint32_t sysconf_param_count = sizeof(sysconf_params) / sizeof(sysconf_params[0]);

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
    params_parse((const char**)sysconf_cmdline.argv, sysconf_cmdline.argc, sysconf_params, sysconf_param_count);
    return 0;
}
