#ifndef __LIGHTENV_SYSCALL__
#define __LIGHTENV_SYSCALL__

#define SYSID_EXIT              0 /* Exit the process */
#define SYSID_CLOSE             1 /* Close a handle */
#define SYSID_READ              2 /* Read from a handle */
#define SYSID_WRITE             3 /* Write to a handle */
#define SYSID_OPEN              4
#define SYSID_OPEN_FILE         5 /* Open a handle to a file */
#define SYSID_OPEN_PROC         6
#define SYSID_OPEN_DRIVER       7
#define SYSID_SEND_IO_CTL       8

#endif // !__LIGHTENV_SYSCALL__
