#ifndef __LIGHTOS_FS_SHARED__
#define __LIGHTOS_FS_SHARED__

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
