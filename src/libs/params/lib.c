#include "params.h"
#include "sys/types.h"

#include <errno.h>
#include <lightos/lib/lightos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void _handle_num_param(cmd_param_t* param, const char* arg)
{
    /* Nope, we want a number */
    uint64_t num;

    if (param->type != CMD_PARAM_TYPE_NUM)
        return;

    num = strtoull(arg, NULL, NULL);

    /* Pick the right destination length */
    switch (param->dest_len) {
    case sizeof(uint8_t):
        *(uint8_t*)param->dest.n = num;
        break;
    case sizeof(uint16_t):
        *(uint16_t*)param->dest.n = num;
        break;
    case sizeof(uint32_t):
        *(uint32_t*)param->dest.n = num;
        break;
    case sizeof(uint64_t):
        *(uint64_t*)param->dest.n = num;
        break;
    }
}

static void _handle_str_param(cmd_param_t* param, const char* arg)
{
    uint32_t arglen = 0;

    if (param->type != CMD_PARAM_TYPE_STRING)
        return;

    /* Find the length of the argument */
    while (arg[1 + arglen] && arg[1 + arglen] != arg[0])
        arglen++;

    if (!arglen)
        return;

    /* Don't copy more than we're allowed to */
    if (arglen > param->dest_len)
        arglen = param->dest_len;

    /* Make sure we copy the right thing */
    arg++;

    strncpy(param->dest.str, arg, arglen);
}

static void _handle_flag_param(const char* arg, cmd_param_t* c_param)
{
    switch (arg[0]) {
    /* Next byte is NULL, this is a boolean */
    case '\0':
        if (c_param->type != CMD_PARAM_TYPE_BOOL)
            break;

        /* User may have forgoten to initialize the bool. Assume 'false' as default in this case */
        if (*c_param->dest.b > 1)
            *c_param->dest.b = 1;
        else
            /* Toggle the boolean, so the user can have both 'false' and 'true' default values */
            *c_param->dest.b = !(*c_param->dest.b);
        break;
        /* Might be eigher a string or a number */
    case '=':
        /* Skip the equals sign */
        arg++;

        /* Check if we want a string */
        if (arg[0] == '\"' || arg[0] == '\'')
            _handle_str_param(c_param, arg);
        else
            _handle_num_param(c_param, arg);

        break;
    }
}

/*!
 * @brief: Tries to find a suitable parameter for a given arg and sets its value
 *
 */
static int _find_param_for_arg(const char* arg, bool is_alias, cmd_param_t* param_list, bool* parsed_list, uint32_t n_param)
{
    cmd_param_t* c_param;
    uint32_t j = 0;
    uint32_t alias_len;

    for (; j < n_param; j++) {
        c_param = &param_list[j];

        /* Param already parsed? Skip */
        if (parsed_list[j])
            continue;

        /* Not an alias, check flag */
        if (!is_alias && arg[0] == c_param->flag) {
            arg++;
            _handle_flag_param(arg, c_param);
            break;
        }

        for (int i = 0; i < (sizeof c_param->aliases / sizeof c_param->aliases[0]); i++) {
            if (!c_param->aliases[i])
                break;

            alias_len = strlen(c_param->aliases[i]);

            /* Check if the arg matches an alias */
            if (strncmp(arg, c_param->aliases[i], alias_len))
                continue;

            /* Skip to the end of the arg */
            arg += alias_len;
            /* Handle the flag */
            _handle_flag_param(arg, c_param);
            break;
        }
    }

    /* Could not find a param for this flag... */
    if (j >= n_param)
        return ENOENT;

    parsed_list[j] = true;
    return 0;
}

/*!
 * @brief: Parse a parameter list
 */
int params_parse(const char** argv, uint32_t argc, cmd_param_t* param_list, uint32_t n_param)
{
    const char* arg;
    BOOL is_alias;
    BOOL* parsed_list;

    if (argc <= 1)
        return ENOENT;

    parsed_list = malloc(n_param);

    if (!parsed_list)
        return -ENOMEM;

    /* Clear the parsed list */
    memset(parsed_list, 0, n_param);

    /* Loop over all given args */
    for (uint32_t i = 1; i < argc; i++) {
        arg = argv[i];
        is_alias = false;

        /* Not a parameter */
        if (arg[0] != '-')
            continue;

        /* Get thing */
        arg++;

        if (arg[0] == '-') {
            is_alias = true;
            arg++;
        }

        (void)_find_param_for_arg(arg, is_alias, param_list, parsed_list, n_param);
    }

    free(parsed_list);
    return 0;
}

LIGHTENTRY int lib_entry()
{
    return 0;
}
