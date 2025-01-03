#ifndef __LIGHTENV_FS_FILE__
#define __LIGHTENV_FS_FILE__

#include "lightos/handle_def.h"
#include "lightos/system.h"

HANDLE open_file(
     const char* path,
    u32 flags,
    u32 mode);

#endif // !__LIGHTENV_FS_FILE__
