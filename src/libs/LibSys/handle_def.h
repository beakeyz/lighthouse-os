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

#define HNDL_INVAL          (-1) /* Tried to get a handle from an invalid source */
#define HNDL_NOT_FOUND      (-2) /* Could not resolve the handle on the kernel side */
#define HNDL_BUSY           (-3) /* Handle target was busy and could not accept the handle at this time */
#define HNDL_TIMED_OUT      (-4) /* Handle target was not marked busy but still didn't accept the handle */
#define HNDL_NO_PERM        (-5) /* No permission to recieve a handle */
#define HNDL_PROTECTED      (-6) /* Handle target is protected and cant have handles right now */

#endif // !__LIGHTENV_HANDLE_DEF__
