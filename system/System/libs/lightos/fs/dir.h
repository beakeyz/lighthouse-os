#ifndef __LIGHTENV_FS_DIR__
#define __LIGHTENV_FS_DIR__

#include "lightos/handle_def.h"
#include "lightos/system.h"
#include <string.h>

typedef struct _DirEntry {
  char name[500];

  uint32_t idx;

  struct _DirEntry* next;
} DirEntry;

static inline bool direntry_is_dir(DirEntry* dentry)
{
  return ((dentry)->name[strlen((dentry)->name)] == '/');
}

typedef struct {
  HANDLE handle;

  DirEntry* entries;
} Directory;

extern Directory* open_dir(
  __IN__ const char* path,
  __IN__ DWORD flags,
  __IN__ DWORD mode
);

extern void close_dir(
 __IN__ Directory* dir
);

extern DirEntry* dir_read_entry(
 __IN__ Directory* dir,
 __IN__ uint32_t idx
);

extern HANDLE dir_entry_open(
 __IN__ DirEntry* entry
);

#endif // !__LIGHTENV_FS_DIR__
