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

typedef struct elf_image {
  struct proc* proc;

  /* ELF header/object stuff */
  struct elf64_hdr* elf_hdr;
  struct elf64_phdr* elf_phdrs;
  struct elf64_dyn* elf_dyntbl;

  size_t elf_dyntbl_mapsize;
  uint32_t elf_symtbl_idx;
  uint32_t elf_dynsym_idx;

  const char* elf_strtab;

  void* kernel_image;
  void* user_base;
  size_t image_size;
} elf_image_t;

extern kerror_t load_elf_image(elf_image_t* image, struct proc* proc, file_t* file);
extern void destroy_elf_image(elf_image_t* image);

/*
 * A dynamic library that has been loaded as support for a dynamic app
 */
typedef struct dynamic_library {
  const char* name;
  const char* path;

  elf_image_t image;

  /* Reference the loaded app to get access to environment info */
  struct loaded_app* app;
  DYNLIB_ENTRY_t entry;

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
  elf_image_t image;

  DYNAPP_ENTRY_t entry;
  
  list_t* library_list;
} loaded_app_t;

extern loaded_app_t* create_loaded_app(file_t* file, struct proc* proc);
extern void destroy_loaded_app(loaded_app_t* app);

extern kerror_t load_app(file_t* file, loaded_app_t** out_app);
extern kerror_t unload_app(loaded_app_t* app);

extern kerror_t register_app(loaded_app_t* app);
extern kerror_t unregister_app(loaded_app_t* app);

static inline uint32_t loaded_app_get_lib_count(loaded_app_t* app)
{
  return app->library_list->m_length;
}

extern void* proc_map_into_kernel(struct proc* proc, vaddr_t uaddr, size_t size);
extern kerror_t loaded_app_set_entry_tramp(loaded_app_t* app);

extern kerror_t _elf_load_phdrs(elf_image_t* image);
extern kerror_t _elf_do_relocations(elf_image_t* image);
extern kerror_t _elf_load_dyn_sections(elf_image_t* image);

#endif // !__ANIVA_DYN_LOADER_PRIV__
