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

enum SYSID {
  SYSID_EXIT, /* Exit the process */
  SYSID_CLOSE, /* Close a handle */
  SYSID_READ, /* Read from a handle */
  SYSID_WRITE, /* Write to a handle */
  SYSID_OPEN,
  SYSID_OPEN_FILE, /* Open a handle to a file */
  SYSID_OPEN_PROC,
  SYSID_OPEN_DRIVER,
  SYSID_SEND_MSG,
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
  /* Open a profile variable on the handle of a profile */
  SYSID_OPEN_PVAR,
  SYSID_GET_PVAR_TYPE,
  SYSID_CREATE_PVAR,
  SYSID_CREATE_DIR,
  /* Manipulate the R/W offset of a handle */
  SYSID_SEEK,
  SYSID_GET_PROCESSTIME,
  SYSID_SLEEP,

  /* TODO: */
  SYSID_LOAD_DRV,
  SYSID_CREATE_PROFILE,
  SYSID_DESTROY_PROFILE,

  /* Device syscalls: Here we pretty much implement all the device endpoints that might be useful to userspace */

  SYSID_DEV_READ,
  SYSID_DEV_WRITE,
  /* HID dev interfacing */
  SYSID_HIDDEV_POLL = 100,
  /* Disk dev interfacing */
  SYSID_DISKDEV_BREAD = 200,
  SYSID_DISKDEV_BWRITE = 201,
};

#define SYS_OK              (0)
#define SYS_INV             (-1)
#define SYS_KERR            (-2)
#define SYS_NOENT           (-3)
#define SYS_NOPERM          (-4)
#define SYS_NULL            (-5)
#define SYS_ERR             (-6)

#endif // !__LIGHTENV_SYSCALL__
