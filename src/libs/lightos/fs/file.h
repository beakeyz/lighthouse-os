#ifndef __LIGHTENV_FS_FILE__
#define __LIGHTENV_FS_FILE__

#include "lightos/handle_def.h"
#include "lightos/system.h"

HANDLE open_file(
    __IN__ const char* path,
    __IN__ DWORD flags,
    __IN__ DWORD mode);

#endif // !__LIGHTENV_FS_FILE__
