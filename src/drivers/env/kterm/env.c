#include "drivers/env/kterm/kterm.h"
#include "libk/string.h"
#include "lightos/var/shared.h"
#include "oss/core.h"
#include "oss/node.h"
#include "oss/obj.h"
#include "system/profile/profile.h"
#include "system/sysvar/var.h"
#include "util.h"

/*!
 * @brief: Itterate function for the variables of a profile
 *
 * TODO: use @arg0 to communicate flags to the Itterate function
 */
static bool print_profile_variable(oss_node_t* node, oss_obj_t* obj, void* param)
{
    sysvar_t* var;

    if (!obj)
        return true;

    var = (sysvar_t*)obj->priv;

    if ((var->flags & SYSVAR_FLAG_HIDDEN) == SYSVAR_FLAG_HIDDEN)
        return true;

    kterm_print(var->key);
    kterm_print(": ");

    switch (var->type) {
    case SYSVAR_TYPE_STRING:
        kterm_println(var->str_value);
        break;
    default:
        kterm_println(to_string(var->qword_value));
    }

    return true;
}

static bool print_profiles(oss_node_t* node, oss_obj_t* obj, void* param)
{
    user_profile_t* profile;

    if (!node || node->type != OSS_PROFILE_NODE)
        return true;

    profile = node->priv;

    if (!profile->name)
        return false;

    kterm_println(profile->name);

    return true;
}

/*!
 * @brief: Print out info about the current profiles and/or their variables
 *
 * If a certain profile is specified, we print that profiles non-hidden variables
 * otherwise, we print all the available profiles
 */
uint32_t kterm_cmd_envinfo(const char** argv, size_t argc)
{
    int error;
    user_profile_t* profile;
    oss_node_t* node;

    switch (argc) {
    case 1:
        error = oss_resolve_node("Runtime", &node);

        if (error)
            return 3;

        error = oss_node_itterate(node, print_profiles, NULL);

        if (error)
            return 3;

        break;
    case 2:
        error = profile_find(argv[1], &profile);

        if (error || !profile)
            return 2;

        /* Print every variable */
        oss_node_itterate(profile->node, print_profile_variable, NULL);
        break;
    default:
        return 1;
    }

    return 0;
}
