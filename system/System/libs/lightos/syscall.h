#ifndef __LIGHTENV_SYSCALL__
#define __LIGHTENV_SYSCALL__

/*
 * This file contains the ids for the various syscalls in the aniva kernel, as well
 * as the different flags that come with them. This header is used both by the kernel,
 * as the userspace, so make sure this stays managable
 *
 * IDs are prefixed with            SYSID_
 * status codes are prefixed with   SYS_
 *
 * TODO: Terminate reduntant syscall ids, due to the new khdriver model
 * TODO: Since SYSID_OPEN requires a HNDL_MODE to be passed, all SYSID_CREATE_*
 * sysids must be terminated
 */

enum SYSID {
    SYSID_INVAL,
    SYSID_EXIT, /* Exit the process */
    SYSID_CLOSE, /* Close a handle */
    SYSID_READ, /* Read from a handle */
    SYSID_WRITE, /* Write to a handle */
    SYSID_OPEN,
    SYSID_OPEN_REL,
    SYSID_SEND_MSG, /* Send a driver message */
    SYSID_SEND_CTL, /* Send a device control code */
    SYSID_CREATE_THREAD,
    SYSID_CREATE_PROC,
    SYSID_CREATE_FILE,

    SYSID_ALLOC_PAGE_RANGE,
    SYSID_DEALLOC_PAGE_RANGE,

    SYSID_SYSEXEC, /* Ask the system to do stuff for us */
    SYSID_DESTROY_THREAD,
    SYSID_DESTROY_PROC,
    SYSID_DESTROY_FILE,
    SYSID_GET_HNDL_TYPE,
    SYSID_GET_SYSVAR_TYPE,
    SYSID_CREATE_SYSVAR,
    /* Directory syscalls */
    SYSID_CREATE_DIR,
    SYSID_DIR_READ,
    SYSID_DIR_FIND,
    /* Manipulate the R/W offset of a handle */
    SYSID_SEEK,
    SYSID_GET_PROCESSTIME,
    SYSID_SLEEP,

    /* TODO: */
    SYSID_UNLOAD_DRV,
    SYSID_CREATE_PROFILE,
    SYSID_DESTROY_PROFILE,

    /* Dynamic loader-specific syscalls */
    SYSID_GET_FUNCADDR,
};

/* Mask that marks a sysid invalid */
#define SYSID_INVAL_MASK 0x80000000

#define SYSID_IS_VALID(id) (((unsigned int)(id) & SYSID_INVAL_MASK) != SYSID_INVAL_MASK)
#define SYSID_GET(id) (enum SYSID)((unsigned int)(id) & SYSID_INVAL_MASK)
#define SYSID_SET_VALID(id, valid) ({if (valid) id &= ~SYSID_INVAL_MASK; else id |= SYSID_INVAL_MASK; })

#define SYS_OK (0)
#define SYS_INV (-1)
#define SYS_KERR (-2)
#define SYS_NOENT (-3)
#define SYS_NOPERM (-4)
#define SYS_NULL (-5)
#define SYS_ERR (-6)

#endif // !__LIGHTENV_SYSCALL__
