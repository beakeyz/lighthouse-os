#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/flow/error.h"
#include "lightos/driver/loader.h"
#include "priv.h"
#include "proc/proc.h"
#include "sched/scheduler.h"
#include <oss/obj.h>

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
          vaddr_t virtual_phdr_base = c_phdr->p_vaddr;
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
        // =)
        break;
    }

  }

  /* Suck my dick */
  return KERR_NONE;
}

static kerror_t _do_load(file_t* file, loaded_app_t* app)
{
  _load_phdrs(file, app);

  //_gather_deps();

  //_load_deps();

  //_finalise_load();
  return KERR_NONE;
}

static inline DYNAPP_ENTRY_t _app_get_entry(loaded_app_t* app)
{
  return (DYNAPP_ENTRY_t)app->elf_hdr->e_entry;
}

/*!
 * @brief: Load a singular dynamically linked executable
 *
 * This will result in a new process wrapped inside a loaded_app struct when 
 * the returnvalue is KERR_NONE
 */
kerror_t load_app(file_t* file, loaded_app_t** out_app)
{
  kernel_panic("TODO: load_app");

  kerror_t error;
  proc_id_t pid;
  proc_t* proc;
  proc_t* cur_proc;
  DYNAPP_ENTRY_t app_entry;
  loaded_app_t* app;

  if (!out_app || !file)
    return -KERR_INVAL;

  /* This gathers the needed ELF info */
  app = create_loaded_app(file);

  /* Assume no memory */
  if (!app)
    return -KERR_NOMEM;

  /* Let's hope lol */
  app_entry = _app_get_entry(app);

  if (!app_entry)
    goto dealloc_and_exit;

  /* Create an addressspsace for this bitch */
  cur_proc = get_current_proc();
  proc = create_proc(cur_proc, &pid, (char*)file->m_obj->name, (FuncPtr)app_entry, NULL, NULL);

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

dealloc_and_exit:
  kernel_panic("load_app failed!");
}

kerror_t unload_app(loaded_app_t* app)
{
  kernel_panic("TODO: unload_app");
}
