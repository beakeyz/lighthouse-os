#ifndef __ANIVA_DYN_LOADER_PRIV__
#define __ANIVA_DYN_LOADER_PRIV__

#include "libk/bin/elf_types.h"
#include "libk/data/hashmap.h"
#include "libk/flow/error.h"
#include <libk/stddef.h>
#include <fs/file.h>

struct proc;
struct loaded_app;

typedef struct dynamic_library {
  const char* name;
  const char* path;

  /* Reference the loaded app to get access to environment info */
  struct loaded_app* app;

  void* image;
  size_t image_size;
} dynamic_library_t;

extern kerror_t load_dynamic_lib(const char* path, struct loaded_app* target_app);
extern kerror_t reload_dynamic_lib(dynamic_library_t* lib, struct loaded_app* target_app);
extern kerror_t unload_dynamic_lib(dynamic_library_t* lib);

typedef struct loaded_app {
  struct proc* proc;

  /* ELF stuff */
  struct elf64_hdr* elf_hdr;
  struct elf64_phdr* elf_phdrs;
  struct elf64_dyn* elf_dyntbl;

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

#endif // !__ANIVA_DYN_LOADER_PRIV__
