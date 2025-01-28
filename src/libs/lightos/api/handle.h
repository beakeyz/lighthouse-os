#ifndef __LIGHTENV_HANDLE_DEF__
#define __LIGHTENV_HANDLE_DEF__

#include <sys/types.h>

typedef int handle_t, HANDLE;

/*
 * Different type of handles a process may have
 * Most handles are going to have an oss object, which will then have it's own underlying
 * type. For special objects that don't live on the oss, like loaded libraries, we make
 * an exception, but they also really should come to live on oss at some point...
 *
 * TODO: get rid of KHNDL and make the kernel just
 * use these -_-
 */
typedef enum HANDLE_TYPE {
    HNDL_TYPE_NONE,
    HNDL_TYPE_OBJECT, /* A handle to a virtual object in the vfs */
    HNDL_TYPE_THREAD,
    /* Shared library when there is a dynamic loaded driver loaded */
    HNDL_TYPE_SHARED_LIB,

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

/*
 * Handle flags
 *
 * HF_*
 */

#define HF_READACCESS (0x0001)
#define HF_WRITEACCESS (0x0002)
#define HF_R (HF_READACCESS)
#define HF_W (HF_WRITEACCESS)
#define HF_RW (HF_READACCESS | HF_WRITEACCESS)

/* Limit this handle to downstream connections (pretty much only used for object lookups by index) */
#define HF_DOWNSTREAM (0x0004)
/*
 * Limit this handle to upstream connections (pretty much only used for object
 * lookups by index). Plz use only one of either downstream or upstream. not both
 */
#define HF_UPSTREAM (0x0008)
/* These guys makes sure the other bit is cleared, so please use the shortend version =) */
#define HF_D (HF_DOWNSTREAM & ~HF_UPSTREAM)
#define HF_U (HF_UPSTREAM & ~HF_DOWNSTREAM)

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
