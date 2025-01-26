#ifndef __LIGHTENV_PROCESS__
#define __LIGHTENV_PROCESS__

#include "lightos/api/filesystem.h"
#include "lightos/api/handle.h"

#include "lightos/types.h"
#include "stdint.h"
#include "sys/types.h"

/*
 * Open a handle to a process which can be used to communicate with this
 * process or use this handle to reference the process to other services
 */
extern HANDLE open_proc(const char* name, u32 flags, u32 mode);
extern HANDLE create_process(const char* name, FuncPtr entry, const char** args, size_t argc, u32 flags);

/*!
 * @brief: Executes a target process and registers it
 *
 * If the process doens't yet have an executable form, the file is passed to provide an executable
 */
extern error_t process_execute(HANDLE handle, File* executable);

/*!
 * @brief: Tries to kill a process pointed to by @handle
 */
extern error_t process_kill(HANDLE handle, u32 flags);

/*!
 * @brief: Gets the time in MS since this process has been running
 *
 */
extern size_t get_process_time(HANDLE handle);

#endif // !__LIGHTENV_PROCESS__
