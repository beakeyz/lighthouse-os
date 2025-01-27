#include "exec.h"
#include "drivers/env/kterm/kterm.h"
#include "libk/flow/error.h"
#include "lightos/api/handle.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "oss/core.h"
#include "oss/object.h"
#include "proc/exec/exec.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include "system/profile/profile.h"
#include "system/sysvar/var.h"

static int _kterm_exec_find_obj(const char* path, oss_object_t** bobj)
{
    int error;
    char* fullpath_buf = nullptr;
    char* c_path;
    char* full_buffer;
    size_t fullpath_len;
    size_t c_path_len;
    size_t path_len;
    size_t total_len;
    sysvar_t* path_var;
    user_profile_t* logged_profile;

    /* Try to find the object with this raw path */
    error = oss_open_object(path, bobj);

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

    /* Get this size */
    error = sysvar_sizeof(path_var, &fullpath_len);

    /* ALlocate a buffer for this */
    fullpath_buf = kmalloc(fullpath_len);

    /* Read this shit */
    if (fullpath_buf)
        error = sysvar_read(path_var, (void*)fullpath_buf, fullpath_len);
    else
        error = -ENOMEM;

    /* Release this reference */
    release_sysvar(path_var);

    /* If there is an error at this point, we've released our sysvar */
    if (error) {
        if (fullpath_buf)
            kfree((void*)fullpath_buf);
        return error;
    }

    /* Duplicate the string */
    c_path = fullpath_buf;
    path_len = strlen(path);

    /* Filter on the path seperator */
    for (uint32_t i = 0; i < (fullpath_len + 1); i++) {
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
        error = oss_open_object(full_buffer, bobj);

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
    error_t error;
    oss_object_t* obj;
    const char* buffer;
    user_profile_t* login_profile;

    if (!argv || !argv[0])
        return 1;

    buffer = argv[0];

    if (buffer[0] == NULL)
        return 2;

    if (_kterm_exec_find_obj(buffer, &obj)) {
        logln("Could not find object!");
        return 3;
    }

    /* Create a process for this guy */
    p = create_proc(argv[0], NULL, NULL);

    /* NOTE: defer the schedule here, since we still need to attach a few handles to the process */
    error = aniva_exec(obj, p, NULL);

    /* Close the object */
    oss_object_close(obj);

    if (error) {
        logln("Could not execute object!");

        destroy_proc(p);
        return 4;
    }

    KLOG_DBG("Got proc: %s %s\n", p->name, cmdline);

    /*
     * Losing this process would be a fatal error because it compromises the entire system
     * :clown: this design is peak stupidity
     */
    ASSERT_MSG(p, "Could not find process! (Lost to the void)");

    /* Make sure the profile has the correct rights */
    kterm_get_login(&login_profile);

    /* Connect the CWD sysvar */
    kterm_connect_cwd_object(p);

    /* Wait for process termination if we don't want to run in the background */
    // if (true)
    ASSERT_MSG(proc_schedule_and_await(p, login_profile, cmdline, "Devices/kterm", HNDL_TYPE_OBJECT, SCHED_PRIO_MID) == 0, "Process termination failed");
    // else
    // proc_schedule(p, SCHED_PRIO_MID);

    /* Make sure we're in terminal right after this exit */
    if (kterm_ismode(KTERM_MODE_GRAPHICS))
        kterm_switch_to_terminal();

    return 0;
}
