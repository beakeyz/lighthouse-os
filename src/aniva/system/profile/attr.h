#ifndef __ANIVA_PROFILE_ATTR_H__
#define __ANIVA_PROFILE_ATTR_H__

#include "libk/stddef.h"

enum PROFILE_TYPE {
    /* This profile has all rights and can even kill kernel processes or load drivers */
    PROFILE_TYPE_ADMIN,
    /*
     * Just below admin, used for tasks that require a deeper view into the system,
     * but have restricted access to mutate opperations
     */
    PROFILE_TYPE_SUPERVISOR,
    /*
     * This profile may do all the things a user expects to be able to do
     * (Read/Write/See certain files, run programs, kill programs, ect.)
     */
    PROFILE_TYPE_USER,
    /*
     * Lowest privilege. May only see and interact with selected objects, may only
     * execute certain syscalls, ect.
     */
    PROFILE_TYPE_LIMITED,

    /* How many profile types we have */
    NR_PROFILE_TYPES,
};

/* Can a profile of a lower type read from this object */
#define PATTR_READ 0x00000001
#define PATTR_WRITE 0x00000002
#define PATTR_SEE 0x00000004
#define PATTR_MOVE 0x00000008
#define PATTR_DELETE 0x00000010
#define PATTR_RENAME 0x00000020
#define PATTR_STAT 0x00000040
#define PATTR_LDDRV 0x00000080
#define PATTR_ULDRV 0x00000100
#define PATTR_SHUTDOWN 0x00000200
#define PATTR_REBOOT 0x00000400
#define PATTR_PROC_KILL 0x00000800

/*
 * In order for an attribute 'executor' (e.g. a process) to have access to an attribute holder
 * through a certain flag, it either has to have a higher profile type, or:
 * Both the executors attributes and the objects attributes have to have matching flags for each
 * others type.
 * So:
 * (a->attr.pflags[b->attr.ptype] & flags) === (b->attr.pflags[a->attr.ptype] & flags)
 */
typedef u32 pattr_flags_t;

/*
 * Very fucking abstract: Just shared attributes
 *
 */
typedef struct pattr {
    /* An objects ptype indicates the minimum profile type needed to access this object */
    enum PROFILE_TYPE ptype;
    /*
     * Pflags will tell us more about what a certain profile may do with this entity
     * When a process from a lower profile wants to access this entity and both its attribute
     * flag and the matching attribute flag on the object is matching, the process is clear to
     * go. This way we can prevent certain actions on oss objects by unprivileged profiles.
     * Profiles of higher privilege than specified in ptype always have permission to do
     * anything
     */
    pattr_flags_t pflags[NR_PROFILE_TYPES];
} pattr_t;

int init_pattr(pattr_t* p_pattr, enum PROFILE_TYPE type, pattr_flags_t attr_flags[NR_PROFILE_TYPES]);
void pattr_clear(pattr_t* attr);
void pattr_mask(pattr_t* attr, pattr_flags_t mask[NR_PROFILE_TYPES]);
bool pattr_hasperm(pattr_t* higher, pattr_t* lower, pattr_flags_t flags);

#endif // !__ANIVA_PROFILE_ATTR_H__
