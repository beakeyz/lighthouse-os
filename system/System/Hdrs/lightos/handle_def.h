#ifndef __LIGHTENV_HANDLE_DEF__
#define __LIGHTENV_HANDLE_DEF__

#include <sys/types.h>

typedef int handle_t, HANDLE;

/*
 * TODO: get rid of KHNDL and make the kernel just
 * use these -_-
 */

typedef enum HANDLE_TYPE {
    HNDL_TYPE_NONE,
    HNDL_TYPE_FILE,
    HNDL_TYPE_DIR,
    HNDL_TYPE_DRIVER,
    HNDL_TYPE_PROC,
    HNDL_TYPE_FS_ROOT,
    HNDL_TYPE_OSS_OBJ, /* A handle to a virtual object in the vfs */
    HNDL_TYPE_KOBJ, /* A handle to a kernel object */
    HNDL_TYPE_THREAD,
    /* Any profile that is present on the system */
    HNDL_TYPE_PROFILE,

    /* Profile variable */
    HNDL_TYPE_SYSVAR,

    /* These types are still to be implemented */
    /* A raw device attached to the device tree on the vfs at :/Devices/ */
    HNDL_TYPE_DEVICE,
    /* Raw buffer, managed by the kernel, stored in the processes environment */
    HNDL_TYPE_BUFFER,
    /* Inter-process pipe interface */
    HNDL_TYPE_UPI_PIPE,
    /* Datastream for contiguous reading / writing of data */
    HNDL_TYPE_STREAM,
    /* Single event */
    HNDL_TYPE_EVENT,
    /* An eventsubscription */
    HNDL_TYPE_EVENTHOOK,
    /* ??? */
    HNDL_TYPE_RESOURCE,
    /* Shared library when there is a dynamic loaded driver loaded */
    HNDL_TYPE_SHARED_LIB,
    /* An entire process environment */
    HNDL_TYPE_PROC_ENV,
    /* A virtual memory mapping */
    HNDL_TYPE_VMEM,

    NR_HNDL_TYPES,
} HANDLE_TYPE,
    handle_type_t;

#define HNDL_INVAL (-1) /* Tried to get a handle from an invalid source */
#define HNDL_NOT_FOUND (-2) /* Could not resolve the handle on the kernel side */
#define HNDL_BUSY (-3) /* Handle target was busy and could not accept the handle at this time */
#define HNDL_TIMED_OUT (-4) /* Handle target was not marked busy but still didn't accept the handle */
#define HNDL_NO_PERM (-5) /* No permission to recieve a handle */
#define HNDL_PROTECTED (-6) /* Handle target is protected and cant have handles right now */

#define HNDL_IS_VALID(handle) (handle >= 0)

/*
 * Type for flags we pass to handle functions
 */
typedef union handle_flags {
    struct {
        /* Reserve 16 bits for handle flags */
        u16 s_flags;
        /* We don't even need this many bytes to represent the type */
        enum HANDLE_TYPE s_type : 16;
        /* Use the remaining bits for a (possibly unused) relative handle */
        HANDLE s_rel_hndl;
    };
    u64 raw;
} handle_flags_t;

static inline handle_flags_t handle_flags(u32 flags, enum HANDLE_TYPE type, HANDLE rel_hndl)
{
    return (handle_flags_t) {
        .s_flags = flags,
        .s_type = type,
        .s_rel_hndl = rel_hndl
    };
}

#define HNDL_FLAG_READACCESS (0x1ULL)
#define HNDL_FLAG_WRITEACCESS (0x2ULL)
#define HNDL_FLAG_R (HNDL_FLAG_READACCESS)
#define HNDL_FLAG_W (HNDL_FLAG_WRITEACCESS)
#define HNDL_FLAG_RW (HNDL_FLAG_READACCESS | HNDL_FLAG_WRITEACCESS)

#define HNDL_FLAG_PERM_MASK 0xff
#define HNDL_FLAG_PERM_OFFSET 23
#define HNDL_FLAGS_GET_PERM(flags) ((flags) >> HNDL_FLAG_PERM_OFFSET) & HNDL_FLAG_PERM_MASK

/*
 * We use this mask to easily clear out any flags that indicate a certain state of the handle
 * Since permissions (Like Read/Write) and state (locked/waiting) are stored in the same ,
 * we want to have this to avoid any confusion
 */
#define HNDL_OPT_MASK (HNDL_FLAG_BUSY | HNDL_FLAG_LOCKED | HNDL_FLAG_WAITING | HNDL_FLAG_INVALID)

/*
 * Handle modes
 */
enum HNDL_MODE {
    /* Normal handle operation: Opens the handle if it exists, fails if not */
    HNDL_MODE_NORMAL,
    /* Same as normal, but creates the object if it does not exist */
    HNDL_MODE_CREATE,
    /* Creates an object, but fails if it already exists */
    HNDL_MODE_CREATE_NEW,
    HNDL_MODE_CURRENT_PROFILE,
    HNDL_MODE_CURRENT_ENV,
};

#endif // !__LIGHTENV_HANDLE_DEF__
