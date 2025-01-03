#ifndef __LIGHTENV_FS_DIR__
#define __LIGHTENV_FS_DIR__

#include "lightos/fs/shared.h"
#include "lightos/handle_def.h"

typedef struct _DirEntry {
    lightos_direntry_t entry;

    uint32_t idx;

    struct _DirEntry* next;
} DirEntry;

static inline bool direntry_is_dir(DirEntry* dentry)
{
    return ((dentry)->entry.type == LIGHTOS_DIRENT_TYPE_DIR);
}

typedef struct {
    HANDLE handle;

    DirEntry* entries;
} Directory;

extern Directory* open_dir(const char* path, u32 flags, u32 mode);

extern void close_dir(Directory* dir);

extern DirEntry* dir_read_entry(Directory* dir, uint32_t idx);

extern HANDLE dir_entry_open(Directory* parent, DirEntry* entry, u32 flags, u32 mode);

#endif // !__LIGHTENV_FS_DIR__
