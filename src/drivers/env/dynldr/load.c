#include "libk/flow/error.h"
#include "priv.h"

/*!
 * @brief: Load a dynamic library from a file into a target app
 *
 * Copies the image into the apps address space and resolves the symbols
 */
kerror_t load_dynamic_lib(const char* path, struct loaded_app* target_app)
{
  kernel_panic("TODO: load_dynamic_lib");
}

kerror_t reload_dynamic_lib(dynamic_library_t* lib, struct loaded_app* target_app)
{
  kernel_panic("TODO: reload_dynamic_lib");
}

kerror_t unload_dynamic_lib(dynamic_library_t* lib)
{
  kernel_panic("TODO: unload_dynamic_lib");
}

kerror_t load_app(file_t* file, loaded_app_t** out_app)
{
  kernel_panic("TODO: load_app");
}

kerror_t unload_app(loaded_app_t* app)
{
  kernel_panic("TODO: unload_app");
}
