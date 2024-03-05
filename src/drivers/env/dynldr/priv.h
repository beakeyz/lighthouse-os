#ifndef __ANIVA_DYN_LOADER_PRIV__
#define __ANIVA_DYN_LOADER_PRIV__

#include "libk/bin/elf_types.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/driver/loader.h"
#include <libk/stddef.h>
#include <fs/file.h>

struct proc;
struct loaded_app;

extern char _app_trampoline[];
extern char _app_trampoline_end[];

typedef int (*APP_ENTRY_TRAMPOLINE_t)(DYNAPP_ENTRY_t main_entry, DYNLIB_ENTRY_t* lib_entries, uint32_t lib_entry_count);

/*
 * A dynamic library that has been loaded as support for a dynamic app
 */
typedef struct dynamic_library {
  const char* name;
  const char* path;

  /* Reference the loaded app to get access to environment info */
  struct loaded_app* app;
  DYNLIB_ENTRY_t entry;

  void* image;
  size_t image_size;

  hashmap_t* dyn_symbols;
  list_t* dependencies;
} dynamic_library_t;

extern kerror_t load_dynamic_lib(const char* path, struct loaded_app* target_app);
extern kerror_t load_dependant_lib(const char* path, dynamic_library_t* library);
extern kerror_t reload_dynamic_lib(dynamic_library_t* lib, struct loaded_app* target_app);
extern kerror_t unload_dynamic_lib(dynamic_library_t* lib);

/*
 * A single loaded app
 *
 * A dynamically linked app which we have loaded through this driver.
 */
typedef struct loaded_app {
  struct proc* proc;

  /* ELF stuff */
  struct elf64_hdr* elf_hdr;
  struct elf64_phdr* elf_phdrs;
  struct elf64_dyn* elf_dyntbl;

  size_t elf_dyntbl_mapsize;

  DYNAPP_ENTRY_t entry;

  /* What is the base of the program in user memory. May be NULL */
  void* image_base;
  /* Where did we load the kernel file buffer */
  void* file_buffer;
  size_t file_size;
  
  hashmap_t* lib_map;
} loaded_app_t;

extern loaded_app_t* create_loaded_app(file_t* file);
extern void destroy_loaded_app(loaded_app_t* app);

extern kerror_t load_app(file_t* file, loaded_app_t** out_app);
extern kerror_t unload_app(loaded_app_t* app);

extern kerror_t register_app(loaded_app_t* app);
extern kerror_t unregister_app(loaded_app_t* app);

static inline uint32_t loaded_app_get_lib_count(loaded_app_t* app)
{
  return app->lib_map->m_size;
}

extern void* loaded_app_map_into_kernel(loaded_app_t* app, vaddr_t uaddr, size_t size);
extern kerror_t loaded_app_set_entry_tramp(loaded_app_t* app);

#endif // !__ANIVA_DYN_LOADER_PRIV__
