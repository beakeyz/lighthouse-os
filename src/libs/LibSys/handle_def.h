#ifndef __LIGHTENV_HANDLE_DEF__
#define __LIGHTENV_HANDLE_DEF__

typedef int                 handle_t;
typedef unsigned char       handle_type_t;

#define HANDLE_t            handle_t
#define HANDLE_TYPE_t       handle_type_t

#define HNDL_TYPE_NONE      (0)
#define HNDL_TYPE_FILE      (1)
#define HNDL_TYPE_DRIVER    (2)
#define HNDL_TYPE_PROC      (3)
#define HNDL_TYPE_FS_ROOT   (4)
#define HNDL_TYPE_VOBJ      (5) /* A handle to a virtual object in the vfs */
#define HNDL_TYPE_KOBJ      (6) /* A handle to a kernel object */
#define HNDL_TYPE_THREAD    (7)

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
#define HNDL_FLAG_RW                (HNDL_FLAG_READACCESS | HNDL_FLAG_WRITEACCESS)
#define HNDL_FLAG_INVALID           (0x8000) /* khandle is not pointing to anything and any accesses to it should be regarded as disbehaviour */

/* 
 * We use this mask to easily clear out any flags that indicate a certain state of the handle 
 * Since permissions (Like Read/Write) and state (locked/waiting) are stored in the same dword,
 * we want to have this to avoid any confusion
 */
#define HNDL_OPT_MASK               (HNDL_FLAG_BUSY | HNDL_FLAG_LOCKED | HNDL_FLAG_WAITING | HNDL_FLAG_INVALID)


#endif // !__LIGHTENV_HANDLE_DEF__
