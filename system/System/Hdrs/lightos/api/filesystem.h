#ifndef __LIGHTOS_FS_SHARED__
#define __LIGHTOS_FS_SHARED__

#include "lightos/api/handle.h"

enum LIGHTOS_DIRENT_TYPE {
    /* Handled as their own things */
    LIGHTOS_DIRENT_TYPE_FILE,
    LIGHTOS_DIRENT_TYPE_DIR,
    /* Handled with oss objects */
    LIGHTOS_DIRENT_TYPE_DEV,
    LIGHTOS_DIRENT_TYPE_DRV,
    LIGHTOS_DIRENT_TYPE_VAR,
    LIGHTOS_DIRENT_TYPE_PROC,
    LIGHTOS_DIRENT_TYPE_OBJ,

    /* ??? */
    LIGHTOS_DIRENT_TYPE_UNKNOWN,
};

static inline HANDLE_TYPE lightos_dirent_get_handletype(enum LIGHTOS_DIRENT_TYPE de_type)
{
    HANDLE_TYPE type = HNDL_TYPE_NONE;

    switch (de_type) {
    case LIGHTOS_DIRENT_TYPE_FILE:
        type = HNDL_TYPE_FILE;
        break;
    case LIGHTOS_DIRENT_TYPE_DIR:
        type = HNDL_TYPE_DIR;
        break;
    case LIGHTOS_DIRENT_TYPE_DEV:
        type = HNDL_TYPE_DEVICE;
        break;
    case LIGHTOS_DIRENT_TYPE_DRV:
        type = HNDL_TYPE_DRIVER;
        break;
    case LIGHTOS_DIRENT_TYPE_VAR:
        type = HNDL_TYPE_SYSVAR;
        break;
    case LIGHTOS_DIRENT_TYPE_PROC:
        type = HNDL_TYPE_PROC;
        break;
    case LIGHTOS_DIRENT_TYPE_OBJ:
        type = HNDL_TYPE_OSS_OBJ;
        break;
    default:
        break;
    }

    return type;
}

static inline const char* lightos_dirent_type_to_string(enum LIGHTOS_DIRENT_TYPE type)
{
    switch (type) {
    case LIGHTOS_DIRENT_TYPE_FILE:
        return "File";
    case LIGHTOS_DIRENT_TYPE_DIR:
        return "Dir";
    case LIGHTOS_DIRENT_TYPE_DEV:
        return "Device";
    case LIGHTOS_DIRENT_TYPE_DRV:
        return "Driver";
    case LIGHTOS_DIRENT_TYPE_VAR:
        return "Sysvar";
    case LIGHTOS_DIRENT_TYPE_PROC:
        return "Process";
    case LIGHTOS_DIRENT_TYPE_OBJ:
        return "Object";
    case LIGHTOS_DIRENT_TYPE_UNKNOWN:
        return "Unknown";
    }

    return "N/A";
}

typedef struct lightos_direntry {
    char name[496];
    enum LIGHTOS_DIRENT_TYPE type;
} lightos_direntry_t;

#endif // ! __LIGHTOS_FS_SHARED__
