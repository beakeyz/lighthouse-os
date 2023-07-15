#ifndef __LIGHTENV_DRIVER__
#define __LIGHTENV_DRIVER__

#include "LibSys/system.h"
#include "LibSys/handle_def.h"

HANDLE_t open_driver(
  __IN__ const char* name,
  __IN__ DWORD flags,
  __IN__ DWORD mode
);


#endif // !__LIGHTENV_DRIVER__
