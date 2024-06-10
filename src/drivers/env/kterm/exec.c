#include "exec.h"
#include "dev/manifest.h"
#include "drivers/env/kterm/kterm.h"
#include "libk/bin/elf.h"
#include "libk/flow/error.h"
#include "lightos/handle_def.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/obj.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"
#include "system/sysvar/map.h"
#include <proc/env.h>

static int _kterm_exec_find_obj(const char* path, oss_obj_t** bobj)
{
    int error;
    char* fullpath_buf;
    char* c_path;
    char* full_buffer;
    size_t c_path_len;
    size_t path_len;
    size_t total_len;
    sysvar_t* path_var;
    user_profile_t* logged_profile;

    error = oss_resolve_obj(path, bobj);

    if (!error)
        return 0;

    logged_profile = nullptr;

    if (kterm_get_login(&logged_profile))
        logged_profile = get_user_profile();

    /* This would be pretty bad lmao */
    if (!logged_profile)
        return -1;

    /* Try to find the path variable */
    if (profile_get_var(logged_profile, "PATH", &path_var))
        return -1;

    /* Get the string value here */
    if (!sysvar_get_str_value(path_var, (const char**)&fullpath_buf))
        return -1;

    /* Duplicate the string */
    fullpath_buf = strdup(fullpath_buf);
    c_path = fullpath_buf;
    path_len = strlen(path);

    /* Filter on the path seperator */
    for (uint32_t i = 0; i < (strlen(fullpath_buf) + 1); i++) {
        char* c_char = fullpath_buf + i;

        /* Cut off the seperator char */
        if (*c_char != PATH_SEPERATOR_CHAR && *c_char)
            continue;

        *c_char = '\0';
        c_path_len = strlen(c_path);
        total_len = c_path_len + 1 + path_len + 2;

        /* Allocate for the full path */
        full_buffer = kmalloc(total_len);

        /* Clear the buffer */
        memset(full_buffer, 0, total_len);

        /* Format yay */
        sfmt(full_buffer, "%s/%s", c_path, path);

        /* Try to search the object */
        error = oss_resolve_obj(full_buffer, bobj);

        /* Free up the buffer */
        kfree(full_buffer);

        /* We found an object here! exit */
        if (!error)
            break;

        /* Next path */
        c_path = c_char + 1;
    }

    kfree(fullpath_buf);
    return error;
}

/*!
 * @brief: Try to execute anything that is the first entry of @argv
 *
 * TODO: Search in respect to the PATH profile variable (Both of the global profile
 * and the currently logged in profile.)
 */
uint32_t kterm_try_exec(const char** argv, size_t argc, const char* cmdline)
{
    proc_t* p;
    const char* buffer;
    user_profile_t* login_profile;

    khandle_t _stdin;
    khandle_t _stdout;
    khandle_t _stderr;

    if (!argv || !argv[0])
        return 1;

    buffer = argv[0];

    if (buffer[0] == NULL)
        return 2;

    oss_obj_t* obj;

    KLOG_DBG("Trying to find object: %s\n", buffer);

    if (_kterm_exec_find_obj(buffer, &obj)) {
        logln("Could not find object!");
        return 3;
    }

    /* Try to grab the file */
    file_t* file = oss_obj_get_file(obj);

    if (!file) {
        logln("Could not execute object!");

        oss_obj_close(obj);
        return 4;
    }

    /* NOTE: defer the schedule here, since we still need to attach a few handles to the process */
    p = elf_exec_64(file, false);

    oss_obj_close(file->m_obj);

    if (!p) {
        logln("Coult not execute object!");
        return 5;
    }

    KLOG_DBG("Got proc: %s\n", p->m_name);

    /*
     * Losing this process would be a fatal error because it compromises the entire system
     * :clown: this design is peak stupidity
     */
    ASSERT_MSG(p, "Could not find process! (Lost to the void)");

    /* Make sure the profile has the correct rights */
    kterm_get_login(&login_profile);

    profile_add_penv(login_profile, p->m_env);

    /* Attach the commandline variable to the profile */
    sysvar_attach(p->m_env->node, "CMDLINE", 0, SYSVAR_TYPE_STRING, NULL, PROFILE_STR(cmdline));

    khandle_type_t driver_type = HNDL_TYPE_DRIVER;
    drv_manifest_t* kterm_manifest = get_driver("other/kterm");

    init_khandle(&_stdin, &driver_type, kterm_manifest);
    init_khandle(&_stdout, &driver_type, kterm_manifest);
    init_khandle(&_stderr, &driver_type, kterm_manifest);

    _stdin.flags |= HNDL_FLAG_READACCESS;
    _stdout.flags |= HNDL_FLAG_WRITEACCESS;
    _stderr.flags |= HNDL_FLAG_RW;

    bind_khandle(&p->m_handle_map, &_stdin);
    bind_khandle(&p->m_handle_map, &_stdout);
    bind_khandle(&p->m_handle_map, &_stderr);

    /* Wait for process termination */
    ASSERT_MSG(proc_schedule_and_await(p, SCHED_PRIO_MID) == 0, "Process termination failed");

    /* Make sure we're in terminal right after this exit */
    if (kterm_ismode(KTERM_MODE_GRAPHICS))
        kterm_switch_to_terminal();

    kterm_println(NULL);

    return 0;
}
