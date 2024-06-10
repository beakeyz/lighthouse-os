#include "file.h"
#include "lightos/handle.h"
#include "lightos/handle_def.h"

HANDLE open_file(const char* path, uint32_t flags, uint32_t mode)
{
    return open_handle(path, HNDL_TYPE_FILE, flags, mode);
}
