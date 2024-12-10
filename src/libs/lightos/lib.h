#ifndef __LIGHTOS_LIB_H__
#define __LIGHTOS_LIB_H__

#include "handle_def.h"
#include "lightos/system.h"
#include "sys/types.h"

/*!
 * @brief: Load a dynamic library from @path
 *
 * Puts the resulting handle in @out_handle and returns TRUE if the
 * library was loaded successfully. Returns FALSE otherwise
 */
extern BOOL lib_load(
     const char* path,
     HANDLE* out_handle);

/*!
 * @brief: Unload a previously loaded library
 *
 * Kill the handle
 */
extern BOOL lib_unload(
     HANDLE handle);

/*!
 * @brief: Opens a loaded library
 *
 * Gets a handle to an already loaded library.
 *
 * TODO: Implement
 */
extern BOOL lib_open(
     const char* path,
     HANDLE* phandle);

/*!
 * @brief: Get the function address of a function in a loaded library
 *
 * @returns: FALSE if something went wrong, TRUE otherwise and puts the
 * function address in @faddr in that case
 */
extern BOOL lib_get_function(
     HANDLE lib_handle,
     const char* func,
     VOID** faddr);

/*!
 * @brief: Gets the load address from the library
 *
 * TODO: Implement
 */
extern VOID* lib_get_load_addr(
     HANDLE lib);

/*!
 * @brief: Gets the load size of the library
 *
 * TODO: Implement
 */
extern u64 lib_get_load_size(
     HANDLE lib);

#endif // !__LIGHTOS_LIB_H__
