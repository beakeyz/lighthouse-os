#ifndef __LIGHTOS_PROC_CMDLINE__
#define __LIGHTOS_PROC_CMDLINE__

/*
 * Header which outlines the cmdline stuff for processes
 * every environment may have a commandline passed with it, which will get
 * put into the CMDLINE variable on the local environment.
 *
 * NOTE: These functions are implemented in cmdline.c
 */

#include <stdint.h>

#define CMDLINE_VARNAME "CMDLINE"
#define CMDLINE_MAX_LEN 1024

/*
 * Top most structure of the commandline
 */
typedef struct lightos_cmdline {
    const char* raw;
    char** argv;
    uint32_t argc;
} lightos_cmdline_t, CMDLINE;

extern int __init_lightos_cmdline();

extern int cmdline_get_len(size_t* bsize);
extern int cmdline_get_raw(char* buffer, size_t bsize);
extern int cmdline_get(CMDLINE* b_cmdline);

extern void cmdline_destroy(CMDLINE* cmdline);

/*
 * LightOS process parameters
 *
 * We can parse the commandline for the userprocess, if they give us a list of pparams.
 * There are two types of params:
 * 1) Flags: These kinds of parameters simply flag the process to do a specific action
 * 2) Selectors: These are specific values baked into the parameter (format: -[name]=[value])
 */
typedef struct lightos_pparam {
    const char** aliases;
    const char** value;
    uint64_t flag_mask;
} lightos_pparam_t, PPARAM;

#endif // !__LIGHTOS_PROC_CMDLINE__
