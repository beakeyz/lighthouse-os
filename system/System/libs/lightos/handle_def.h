#ifndef __LIGHTENV_HANDLE_DEF__
#define __LIGHTENV_HANDLE_DEF__

typedef int                 handle_t;
typedef unsigned char       handle_type_t;

#define HANDLE              handle_t
#define HANDLE_TYPE         handle_type_t

/*
 * TODO: get rid of KHNDL and make the kernel just 
 * use these -_-
 */

#define HNDL_TYPE_NONE              (0)
#define HNDL_TYPE_FILE              (1)
#define HNDL_TYPE_DIR               (2)
#define HNDL_TYPE_DRIVER            (3)
#define HNDL_TYPE_PROC              (4)
#define HNDL_TYPE_FS_ROOT           (5)
#define HNDL_TYPE_OSS_OBJ           (6) /* A handle to a virtual object in the vfs */
#define HNDL_TYPE_KOBJ              (7) /* A handle to a kernel object */
#define HNDL_TYPE_THREAD            (8)
/* Any profile that is present on the system */
#define HNDL_TYPE_PROFILE           (9)
  #define HNDL_MODE_CURRENT_PROFILE (0)
  #define HNDL_MODE_SCAN_PROFILE    (1)

/* Profile variable */
#define HNDL_TYPE_SYSVAR            (10)

/* These types are still to be implemented */
/* A raw device attached to the device tree on the vfs at :/Devices/ */
#define HNDL_TYPE_DEVICE            (11)
/* Raw buffer, managed by the kernel */
#define HNDL_TYPE_BUFFER            (12)
/* Inter-process interface */
#define HNDL_TYPE_IP_INTERF         (13)
/* Datastream for contiguous reading / writing of data */
#define HNDL_TYPE_STREAM            (14)
/* Single event */
#define HNDL_TYPE_EVENT             (15)
/* An eventsubscription */
#define HNDL_TYPE_EVENTHOOK         (16)
/* ??? */
#define HNDL_TYPE_RESOURCE          (17)
/* Shared library when there is a dynamic loaded driver loaded */
#define HNDL_TYPE_SHARED_LIB        (18)
/* An entire process environment */
#define HNDL_TYPE_PROC_ENV          (19)

#define HNDL_INVAL          (-1) /* Tried to get a handle from an invalid source */
#define HNDL_NOT_FOUND      (-2) /* Could not resolve the handle on the kernel side */
#define HNDL_BUSY           (-3) /* Handle target was busy and could not accept the handle at this time */
#define HNDL_TIMED_OUT      (-4) /* Handle target was not marked busy but still didn't accept the handle */
#define HNDL_NO_PERM        (-5) /* No permission to recieve a handle */
#define HNDL_PROTECTED      (-6) /* Handle target is protected and cant have handles right now */

#define HNDL_FLAG_BUSY              (0x0001) /* khandle is busy and we have given up waiting for it */
#define HNDL_FLAG_WAITING           (0x0002) /* khandle is busy but we are waiting for it to be free so opperations on this handle can be queued if needed */
#define HNDL_FLAG_LOCKED            (0x0004) /* khandle is locked by the kernel and can't be opperated by userspace (think of shared libraries and stuff) */
#define HNDL_FLAG_READACCESS        (0x0008)
#define HNDL_FLAG_WRITEACCESS       (0x0010)
#define HNDL_FLAG_R                 (HNDL_FLAG_READACCESS)
#define HNDL_FLAG_W                 (HNDL_FLAG_WRITEACCESS)
#define HNDL_FLAG_RW                (HNDL_FLAG_READACCESS | HNDL_FLAG_WRITEACCESS)
#define HNDL_FLAG_INVALID           (0x8000) /* khandle is not pointing to anything and any accesses to it should be regarded as disbehaviour */

/* 
 * We use this mask to easily clear out any flags that indicate a certain state of the handle 
 * Since permissions (Like Read/Write) and state (locked/waiting) are stored in the same dword,
 * we want to have this to avoid any confusion
 */
#define HNDL_OPT_MASK               (HNDL_FLAG_BUSY | HNDL_FLAG_LOCKED | HNDL_FLAG_WAITING | HNDL_FLAG_INVALID)


#endif // !__LIGHTENV_HANDLE_DEF__
