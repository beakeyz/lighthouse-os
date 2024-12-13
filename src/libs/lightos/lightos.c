#include <lightos/error.h>
#include <lightos/lightos.h>
#include <lightos/proc/cmdline.h>

static u32 __lightos_status_flags;

LEXPORT void __init_libc(void);
LEXPORT int __init_lightos_cmdline();
LEXPORT int __init_devices();
LEXPORT int __init_errstack();

LEXPORT error_t __lightos_disable_errstack();
LEXPORT error_t __lightos_enable_errstack();

BOOL lightos_has_cmdline()
{
    return (__lightos_status_flags & LIGHTOS_SFLAGS_NOCMDLINE) != LIGHTOS_SFLAGS_NOCMDLINE;
}

BOOL lightos_has_devices()
{
    return (__lightos_status_flags & LIGHTOS_SFLAGS_NODEVICES) != LIGHTOS_SFLAGS_NODEVICES;
}

BOOL lightos_has_errstack()
{
    return (__lightos_status_flags & LIGHTOS_SFLAGS_NOERRSTACK) != LIGHTOS_SFLAGS_NOERRSTACK;
}

error_t lightos_enable_errstack()
{
    if (lightos_has_errstack())
        return -EISENABLED;

    __lightos_status_flags &= ~LIGHTOS_SFLAGS_NOERRSTACK;

    return __lightos_enable_errstack();
}

error_t lightos_disable_errstack()
{
    if (!lightos_has_errstack())
        return -EISDISABLED;

    __lightos_status_flags |= LIGHTOS_SFLAGS_NOERRSTACK;

    return __lightos_disable_errstack();
}

int __init_lightos()
{
    int error;

    error = __init_lightos_cmdline();

    if (error)
        __lightos_status_flags |= LIGHTOS_SFLAGS_NOCMDLINE;

    error = __init_devices();

    if (error)
        __lightos_status_flags |= LIGHTOS_SFLAGS_NODEVICES;

    error = __init_errstack();

    if (error)
        __lightos_status_flags |= LIGHTOS_SFLAGS_NOERRSTACK;

    return error;
}

LIGHTEXIT int __exit_lightos()
{
    // TODO
    return 0;
}

/*!
 * @brief: Dynamic entrypoint for the c (or rather lightos) runtime library
 */
LIGHTENTRY int libentry()
{
    __lightos_status_flags = 0;

    /*
     * Initialize libc things
     * This has a strict fatal failure policy, which calls sys_exit, if any part of
     * this initialization code fails...
     *
     * If we get to __init_lightos, the entirety of libc SHOULD be setup without
     * error =D
     */
    __init_libc();

    /* Init the rest of the lightos runtime library */
    (void)__init_lightos();

    return 0;
}
