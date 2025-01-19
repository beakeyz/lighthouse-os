#ifndef __LIGHTOS_OBJECTS_H__
#define __LIGHTOS_OBJECTS_H__

#include "lightos/api/handle.h"

/*
 * API file for the oss
 *
 * We'll lay out some important shared stuff of objects here
 */

enum OSS_OBJECT_TYPE {
    OT_ALREADY_SET = -2,
    OT_INVALID = -1,
    OT_NONE,
    OT_FILE,
    OT_DIR,
    OT_DEVICE,
    OT_DGROUP,
    OT_DRIVER,
    OT_PROCESS,
    OT_THREAD,
    OT_PROFILE,
    OT_SYSVAR,
    OT_KEVENT,
    OT_GENERIC,

    NR_OBJECT_TYPES
};

/* Object flags */
#define OF_PROPEGATE 0x0001 // Propegate some object calls
#define OF_NO_DISCON 0x0002 // Object can't be disconnected by anyone
#define OF_NO_CON 0x0004 // Object can't be connected by anyone
#define OF_READONLY 0x0008 // An object can be only readable (by userspace)

/*
 * User struct to represent generic oss objects
 *
 * Tells userspace everything it needs to know about an object in order to talk
 * to the kernel about it
 */
typedef struct object {
    /*
     * This is the key this object is identified with
     * FIXME: Make this guy just a const char*
     */
    char key[64];
    /* Handle to the kernel object */
    HANDLE handle;
    /* Type of this object */
    enum OSS_OBJECT_TYPE type;
} Object, object_t;

#endif // !__LIGHTOS_OBJECTS_H__
