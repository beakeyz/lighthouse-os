#ifndef __LIGHTOS_OBJECTS_H__
#define __LIGHTOS_OBJECTS_H__

/*
 * API file for the oss
 *
 * We'll lay out some important shared stuff of objects here
 */

enum OSS_OBJECT_TYPE {
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
    OT_GENERIC,

    NR_OBJECT_TYPES
};

/* Object flags */
#define OF_PROPEGATE 0x0001 // Propegate some object calls
#define OF_NO_DISCON 0x0002 // Object can't be disconnected by anyone
#define OF_NO_CON 0x0004 // Object can't be connected by anyone

#endif // !__LIGHTOS_OBJECTS_H__
