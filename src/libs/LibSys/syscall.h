#ifndef __LIGHTENV_SYSCALL__
#define __LIGHTENV_SYSCALL__

/*
 * This file contains the ids for the various syscalls in the aniva kernel, as well
 * as the different flags that come with them. This header is used both by the kernel,
 * as the userspace, so make sure this stays managable
 *
 * IDs are prefixed with            SYSID_
 * flags are prefixed with          F_
 * status codes are prefixed with   SYS_
 */

#define SYSID_EXIT              0 /* Exit the process */
#define SYSID_CLOSE             1 /* Close a handle */
#define SYSID_READ              2 /* Read from a handle */
#define SYSID_WRITE             3 /* Write to a handle */
#define SYSID_OPEN              4
#define SYSID_OPEN_FILE         5 /* Open a handle to a file */
#define SYSID_OPEN_PROC         6
#define SYSID_OPEN_DRIVER       7
#define SYSID_SEND_IO_CTL       8
#define SYSID_CREATE_THREAD     9
#define SYSID_CREATE_PROC       10
#define SYSID_CREATE_FILE       11

#define SYSID_ALLOCATE_PAGES    12

/* This is used by libc malloc */
#define F_MEM_MALLOC            (0x00000001)
/* Protect this region from external I/O */
#define F_MEM_PROTECT           (0x00000002)

#define SYSID_SYSEXEC           13 /* Ask the system to do stuff for us */
#define SYSID_DESTROY_THREAD    14
#define SYSID_DESTROY_PROC      15
#define SYSID_DESTROY_FILE      16
#define SYSID_GET_HNDL_TYPE     17
/* Open a profile variable on the handle of a profile */
#define SYSID_OPEN_PVAR         18
#define SYSID_GET_PVAR_TYPE     19
#define SYSID_CREATE_PVAR       20
#define SYSID_CREATE_DIR        21
/* Manipulate the R/W offset of a handle */
#define SYSID_SEEK              22
#define SYSID_GET_PROCESSTIME   23
#define SYSID_SLEEP             24

#define SYS_OK              (0)
#define SYS_INV             (-1)
#define SYS_KERR            (-2)
#define SYS_NOENT           (-3)
#define SYS_NOPERM          (-4)
#define SYS_NULL            (-5)
#define SYS_ERR             (-6)

#endif // !__LIGHTENV_SYSCALL__
