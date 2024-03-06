#include "fs/file.h"
#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "mem/heap.h"
#include "priv.h"
#include "proc/proc.h"
#include "proc/profile/profile.h"
#include "proc/profile/variable.h"
#include "sched/scheduler.h"
#include <oss/obj.h>

#define SYMFLAG_ORIG_APP 0x01
#define SYMFLAG_ORIG_LIB 0x02
#define SYMFLAG_EXPORTED 0x04

/*
 * A single symbol that we have loaded in a context
 */
typedef struct loaded_sym {
  const char* symname;
  uint8_t flags;
  union {
    loaded_app_t* app;
    dynamic_library_t* lib;
  };
} loaded_sym_t;

/*
 * Context of the current load state
 *
 * This struct keeps track of all the symbols we've already loaded and know the location of, both
 * in the kernel memory as in the processes memory. 
 * 
 * Never allocated on the heap
 */
typedef struct load_ctx {

} load_ctx_t;

static dynamic_library_t* _create_dynamic_lib(loaded_app_t* parent, const char* name, const char* path)
{
  dynamic_library_t* ret;

  ret = kmalloc(sizeof(*ret));
  
  if (!ret)
    return nullptr;

  memset(ret, 0, sizeof(*ret));

  ret->app = parent;
  ret->name = strdup(name);
  ret->path = strdup(path);
  ret->dependencies = init_list();

  return ret;
}

static void _destroy_dynamic_lib(dynamic_library_t* lib)
{
  if (lib->dyn_symbols)
    destroy_hashmap(lib->dyn_symbols);

  destroy_list(lib->dependencies);
  kfree((void*)lib->name);
  kfree((void*)lib->path);
  kfree(lib);
}

static inline const char* _append_path_to_searchdir(const char* search_dir, const char* path)
{
  char* search_path;
  size_t search_path_len;

  search_path_len = strlen(search_dir) + 1 + strlen(path) + 1;
  search_path = kmalloc(search_path_len);

  if (!search_path)
    return nullptr;

  /* Clear */
  memset(search_path, 0, search_path_len);

  /* Copy search dir */
  strncpy(search_path, search_dir, strlen(search_dir));
  /* Set the path seperator */
  search_path[strlen(search_dir)] = '/';
  /* Copy the lib path */
  strncpy(&search_path[strlen(search_dir)+1], path, strlen(path));

  return search_path;
}


/*!
 * @brief: Do essensial relocations
 *
 * Simply finds relocation section headers and does ELF section relocating
 */
static kerror_t _do_relocations(struct elf64_hdr* hdr)
{
  struct elf64_shdr* shdr;
  struct elf64_rela* table;
  struct elf64_shdr* target_shdr;
  struct elf64_sym* symtable_start;
  size_t rela_count;

  /* Walk the section headers */
  for (uint32_t i = 0; i < hdr->e_shnum; i++) {
    shdr = elf_get_shdr(hdr, i);

    if (shdr->sh_type != SHT_RELA)
      continue;

    /* Get the rel table of this section */
    table = (struct elf64_rela*)shdr->sh_addr;

    /* Get the target section to apply these reallocacions to */
    target_shdr = elf_get_shdr(hdr, shdr->sh_info);

    /* Get the start of the symbol table */
    symtable_start = (struct elf64_sym*)elf_get_shdr(hdr, shdr->sh_link)->sh_addr;

    /* Compute the amount of relocations */
    rela_count = ALIGN_UP(shdr->sh_size, sizeof(struct elf64_rela)) / sizeof(struct elf64_rela);

    for (uint32_t i = 0; i < rela_count; i++) {
      struct elf64_rela* current = &table[i];

      /* Where we are going to change stuff */
      vaddr_t P = target_shdr->sh_addr + current->r_offset;

      /* The value of the symbol we are referring to */
      vaddr_t S = symtable_start[ELF64_R_SYM(table[i].r_info)].st_value;
      vaddr_t A = current->r_addend;

      size_t size = 0;
      vaddr_t val = S + A;

      switch (ELF64_R_TYPE(current->r_info)) {
        case R_X86_64_64:
          size = 8;
          break;
        case R_X86_64_32S:
        case R_X86_64_32:
          size = 4;
          break;
        case R_X86_64_PC32:
        case R_X86_64_PLT32:
          val -= P;
          size = 4;
          break;
        case R_X86_64_PC64:
          val -= P;
          size = 8;
          break;
        default:
          size = 0;
          break;
      }

      if (!size)
        return -KERR_INVAL;

      memcpy((void*)P, &val, size);
    }
  }

  return KERR_NONE;
}

/*!
 * @brief: Scan the symbols in a given elf header and grab/resolve it's symbols
 *
 * When we fail to find a symbol, this function fails and returns -KERR_INVAL
 */
static kerror_t _do_symbols(struct elf64_hdr* hdr, uint32_t symhdr_idx)
{
  uint32_t sym_count;
  struct elf64_shdr* shdr = elf_get_shdr(hdr, symhdr_idx);

  /* Grab the symbol count */
  sym_count = shdr->sh_size / sizeof(struct elf64_sym);

  /* Grab the start of the symbol name table */
  char* names = (char*)elf_get_shdr(hdr, shdr->sh_link)->sh_addr;

  /* Grab the start of the symbol table */
  struct elf64_sym* sym_table_start = (struct elf64_sym*)shdr->sh_addr;

  /* Walk the table and resolve any symbols */
  for (uint32_t i = 0; i < sym_count; i++) {
    struct elf64_sym* current_symbol = &sym_table_start[i];
    char* sym_name = names + current_symbol->st_name;

    switch (current_symbol->st_shndx) {
      case SHN_UNDEF:
        /*
         * Need to look for this symbol in an earlier loaded binary =/ 
         * TODO: Create a load context that keeps track of symbol addresses, origins, ect.
         */
        kernel_panic("TODO: found an unresolved symbol!");
        break;
      default:
        {
          struct elf64_shdr* _hdr = elf_get_shdr(hdr, current_symbol->st_shndx);
          current_symbol->st_value += _hdr->sh_addr;

          break;
        }
    }
  }

  return 0;
}

/*!
 * @brief: Load a dynamic library from a file into a target app
 *
 * Copies the image into the apps address space and resolves the symbols, after
 * we've loaded the dependencies for this library
 */
kerror_t load_dynamic_lib(const char* path, struct loaded_app* target_app)
{
  const char* search_path;
  const char* search_dir;
  file_t* lib_file;
  dynamic_library_t* lib;
  profile_var_t* libs_var;

  profile_scan_var(LIBSPATH_VARPATH, NULL, &libs_var);
  profile_var_get_str_value(libs_var, &search_dir);

  lib = nullptr;
  search_path = _append_path_to_searchdir(search_dir, path);

  if (!search_path)
    return -KERR_NOMEM;

  /* First, try to create a library object */
  lib = _create_dynamic_lib(target_app, path, search_path);

  kfree((void*)search_path);

  if (!lib)
    return -KERR_INVAL;

  /* Then try to open the target file */
  lib_file = file_open(lib->path);

  if (!lib_file)
    goto dealloc_and_exit;

  /*
   * Now we do epic loading or some shit idk
   */
  printf("Trying to load dynamic library %s\n", lib_file->m_obj->name);
  kernel_panic("TODO: load_dynamic_lib");

  

dealloc_and_exit:
  if (lib)
    _destroy_dynamic_lib(lib);

  return -KERR_INVAL;
}

kerror_t reload_dynamic_lib(dynamic_library_t* lib, struct loaded_app* target_app)
{
  kernel_panic("TODO: reload_dynamic_lib");
}

kerror_t unload_dynamic_lib(dynamic_library_t* lib)
{
  kernel_panic("TODO: unload_dynamic_lib");
}

/*!
 * @brief: Itterate the program headers and load shit we need
 *
 * NOTE: We assume PT_INTERP is correct
 */
static kerror_t _load_phdrs(file_t* file, loaded_app_t* app)
{
  proc_t* proc;
  struct elf64_phdr* c_phdr;

  proc = app->proc;

  for (uint32_t i = 0; i < app->elf_hdr->e_phnum; i++) {
    c_phdr = &app->elf_phdrs[i];

    switch (c_phdr->p_type) {
      case PT_LOAD:
        {
          vaddr_t virtual_phdr_base = (vaddr_t)app->image_base + c_phdr->p_vaddr;
          size_t phdr_size = c_phdr->p_memsz;

          vaddr_t v_user_phdr_start = Must(__kmem_alloc_range(
                proc->m_root_pd.m_root,
                proc->m_resource_bundle,
                virtual_phdr_base,
                phdr_size,
                KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER | KMEM_CUSTOMFLAG_NO_REMAP,
                KMEM_FLAG_WRITABLE
                ));


          vaddr_t v_kernel_phdr_start = Must(kmem_get_kernel_address(v_user_phdr_start, proc->m_root_pd.m_root));

          /* Then, zero the rest of the buffer */
          /* TODO: ??? */
          memset((void*)(v_kernel_phdr_start), 0, phdr_size);

          /*
           * Copy elf into the mapped area 
           */
          elf_read(
              file,
              (void*)v_kernel_phdr_start,
              c_phdr->p_filesz > c_phdr->p_memsz ?
                &c_phdr->p_memsz :
                &c_phdr->p_filesz,
              c_phdr->p_offset);
        }
        break;
      case PT_DYNAMIC:
        /* Remap to the kernel */
        app->elf_dyntbl_mapsize = ALIGN_UP(c_phdr->p_memsz, SMALL_PAGE_SIZE);
        app->elf_dyntbl = loaded_app_map_into_kernel(app, (vaddr_t)(app->image_base + c_phdr->p_vaddr), app->elf_dyntbl_mapsize);
        break;
    }
  }

  /* Suck my dick */
  return KERR_NONE;
}

/*!
 * @brief: Cache info about the dynamic sections
 *
 * Loop over the dynamic sections
 */
static kerror_t _load_dyn_sections(file_t* file, loaded_app_t* app)
{
  char* strtab;
  struct elf64_dyn* dyns_entry;

  strtab = nullptr;
  dyns_entry = (struct elf64_dyn*)Must(kmem_get_kernel_address((vaddr_t)app->elf_dyntbl, app->proc->m_root_pd.m_root));

  /* The dynamic section table is null-terminated xD */
  while (dyns_entry->d_tag) {

    /* First look for anything that isn't the DT_NEEDED type */
    switch (dyns_entry->d_tag) {
      case DT_STRTAB:
        /* We can do this, since we have assured a kernel mapping through loading this pheader beforehand */
        strtab = (char*)Must(kmem_get_kernel_address((vaddr_t)app->image_base + dyns_entry->d_un.d_ptr, app->proc->m_root_pd.m_root));
        break;
      case DT_SYMTAB:
        break;
      case DT_PLTGOT:
      case DT_PLTREL:
      case DT_PLTRELSZ:
        break;
    }

    /* Next */
    dyns_entry++;
  }

  /* Didn't find the needed dynamic sections */
  if (!strtab)
    return -KERR_INVAL;

  /* Reset the itterator */
  dyns_entry = app->elf_dyntbl;

  while (dyns_entry->d_tag) {

    if (dyns_entry->d_tag != DT_NEEDED)
      goto cycle;

    /* Load the library */
    load_dynamic_lib((const char*)(strtab + dyns_entry->d_un.d_val), app);

cycle:
    dyns_entry++;
  }

  return KERR_NONE;
}

static kerror_t _do_load(file_t* file, loaded_app_t* app)
{
  /* Read the program headers */
  _load_phdrs(file, app);

  printf("Trying to load app yay\n");
  _load_dyn_sections(file, app);
  //_gather_deps();

  //_load_deps();

  //_finalise_load();
  return KERR_NONE;
}

/*!
 * @brief: Load a singular dynamically linked executable
 *
 * This will result in a new process wrapped inside a loaded_app struct when 
 * the returnvalue is KERR_NONE
 */
kerror_t load_app(file_t* file, loaded_app_t** out_app)
{
  kerror_t error;
  proc_id_t pid;
  proc_t* proc;
  proc_t* cur_proc;
  loaded_app_t* app;

  if (!out_app || !file)
    return -KERR_INVAL;

  /* This gathers the needed ELF info */
  app = create_loaded_app(file);

  /* Assume no memory */
  if (!app)
    return -KERR_NOMEM;

  /* Create an addressspsace for this bitch */
  cur_proc = get_current_proc();
  proc = create_proc(cur_proc, &pid, (char*)file->m_obj->name, NULL, NULL, NULL);

  app->proc = proc;

  /*
   * We've loaded the main object. Now, let's:
   * 0) Find a place to actually load this shit :clown:
   * 1) Relocate the static parts of the binary
   * 2) Find the libraries we need to load and load them in the correct order
   * 3) Execute the process
   */

  error = _do_load(file, app);

  /* Idk */
  ASSERT(KERR_OK(error));

//dealloc_and_exit:
  //kernel_panic("load_app failed!");
  return KERR_NONE;
}

kerror_t unload_app(loaded_app_t* app)
{
  kernel_panic("TODO: unload_app");
}
