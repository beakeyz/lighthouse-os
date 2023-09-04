#include "loader.h"
#include "dev/core.h"
#include "dev/debug/serial.h"
#include "dev/driver.h"
#include "dev/external.h"
#include "dev/manifest.h"
#include "fs/vobj.h"
#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/bin/ksyms.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include <fs/vfs.h>

struct loader_ctx {
  const char* path;
  struct elf64_hdr* hdr; 
  struct elf64_shdr* shdrs;
  char* section_strings;
  char* strtab;
  extern_driver_t* driver;

  size_t size;
  size_t sym_count;

  uint32_t expdrv_idx, symtab_idx;
};

static ErrorOrPtr __fixup_section_headers(struct loader_ctx* ctx)
{
  for (uint32_t i = 0; i < ctx->hdr->e_shnum; i++) {
    struct elf64_shdr* shdr = elf_get_shdr(ctx->hdr, i);

    switch (shdr->sh_type) {
      case SHT_NOBITS:
        {
          if (!shdr->sh_size)
            break;

          /* Just give any NOBITS sections a bit of memory */
          TRY(result, __kmem_alloc_range(nullptr, ctx->driver->m_manifest->m_resources, HIGH_MAP_BASE, shdr->sh_size, NULL, NULL));

          shdr->sh_addr = result;
          break;
        }
      default:
        shdr->sh_addr = (uintptr_t)ctx->hdr + shdr->sh_offset;
        break;
    }
  }

  return Success(0);
}

static ErrorOrPtr __do_driver_relocations(struct loader_ctx* ctx)
{
  struct elf64_shdr* shdr;
  struct elf64_rela* table;
  struct elf64_shdr* target_shdr;
  struct elf64_sym* symtable_start;
  size_t rela_count;

  /* Walk the section headers */
  for (uint32_t i = 0; i < ctx->hdr->e_shnum; i++) {
    shdr = elf_get_shdr(ctx->hdr, i);

    if (shdr->sh_type != SHT_RELA)
      continue;

    /* Get the rel table of this section */
    table = (struct elf64_rela*)shdr->sh_addr;

    /* Get the target section to apply these reallocacions to */
    target_shdr = elf_get_shdr(ctx->hdr, shdr->sh_info);

    /* Get the start of the symbol table */
    symtable_start = (struct elf64_sym*)elf_get_shdr(ctx->hdr, shdr->sh_link)->sh_addr;

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
        return Error();

      memcpy((void*)P, &val, size);
    }
  }

  return Success(0);
}

static ErrorOrPtr __resolve_kernel_symbols(struct loader_ctx* ctx)
{
  struct elf64_shdr* shdr = &ctx->shdrs[ctx->symtab_idx];

  /* Grab the symbol count */
  ctx->sym_count = shdr->sh_size / sizeof(struct elf64_sym);

  /* Grab the start of the symbol name table */
  char* names = (char*)elf_get_shdr(ctx->hdr, shdr->sh_link)->sh_addr;

  /* Grab the start of the symbol table */
  struct elf64_sym* sym_table_start = (struct elf64_sym*)shdr->sh_addr;

  /* Walk the table and resolve any symbols */
  for (uint32_t i = 0; i < ctx->sym_count; i++) {
    struct elf64_sym* current_symbol = &sym_table_start[i];
    char* sym_name = names + current_symbol->st_name;

    switch (current_symbol->st_shndx) {
      case SHN_UNDEF:

        current_symbol->st_value = get_ksym_address(sym_name);

        /* Oops, invalid symbol: skip */
        if (!current_symbol->st_value)
          break;

        ctx->driver->m_ksymbol_count++;
        break;
      default:
        {
          struct elf64_shdr* hdr = elf_get_shdr(ctx->hdr, current_symbol->st_shndx);
          current_symbol->st_value += hdr->sh_addr;
        }
    }
  }

  return Success(0);
}

static ErrorOrPtr __move_driver(struct loader_ctx* ctx)
{

  /* First, make sure any values and addresses are mapped correctly */
  if (IsError(__fixup_section_headers(ctx)))
    return Error();


  if (IsError(__resolve_kernel_symbols(ctx)))
    return Error();


  /* Then handle any pending relocations */
  if (IsError(__do_driver_relocations(ctx)))
    return Error();


  /* Find the driver structure */


  return Success(0);
}

/*
 * Do basic checks on the ELF and the driver which should be 
 * embedded in the file. If we can't find or validate it, we fail
 * and abort the load.
 * These checks are similar to the ones linux does
 */
static int __check_driver(struct loader_ctx* ctx)
{
  uint32_t i;
  uint32_t expdrv_sections = 0,
           symtab_sections = 0;
  struct elf64_shdr* shdr;

  if (!memcmp(ctx->hdr->e_ident, ELF_MAGIC, ELF_MAGIC_LEN))
    return -1;

  if (ctx->hdr->e_ident[EI_CLASS] != ELF_CLASS_64)
    return -1;

  if (ctx->hdr->e_type != ET_REL)
    return -1;

  if (ctx->hdr->e_shentsize != sizeof(struct elf64_shdr))
    return -1;

  if (!ctx->hdr->e_shstrndx)
    return -1;

  if (ctx->hdr->e_shstrndx >= ctx->hdr->e_shnum)
    return -1;

  /* Probably a valid elf, let's grab some funny data */
  ctx->shdrs = (void*)ctx->hdr + ctx->hdr->e_shoff;
  ctx->section_strings = (void*)ctx->hdr + ctx->shdrs[ctx->hdr->e_shstrndx].sh_offset;

  for (i = 1; i < ctx->hdr->e_shnum; i++) {
    shdr = &ctx->shdrs[i];

    switch (shdr->sh_type) {
      case SHT_SYMTAB:
        {
          ctx->symtab_idx = i;
          symtab_sections++;
        }
      default:

        /* Validate section */

        if (strcmp(ctx->section_strings + shdr->sh_name, ".expdrv") == 0) {
          /* TODO: real validation */

          if (shdr->sh_size != sizeof(aniva_driver_t)) {
            kernel_panic("Size check failed while loading external driver: could it be that dependencies enlarge the sections size?");
            return -1;
          }

          expdrv_sections++;
          ctx->expdrv_idx = i;
        }
        break;
    }
  }

  if (expdrv_sections != 1 || symtab_sections != 1)
    return -1;

  return 0;
}

/*
 * Detect certain aspects of drivers, like
 * 
 * o do they have msg functions (ioctls)
 * o do they provide non-trivial service
 * o do they claim to spawn any processes (deamons)
 * o do they want to run in userspace (is this a userspace driver actually?)
 * o ...
 *
 */
static void __detect_driver_attributes(dev_manifest_t* manifest)
{
  /* TODO: */
  (void)manifest;
}

/*
 * We need to check if the functions that the driver claims to have are valid
 * functions within the ELF
 *
 * At this point it is verified that the .expdrv section *probably* contains an
 * aniva_driver_t struct, which is implied by the size of the section, but we need to ensure that
 * there are valid function pointers in the struct that we got, and that these functions are valid 
 * within the ELF
 *
 * We do this by checking if the address that the function pointers contain, points to a valid symbol.
 * If they do, we consider the driver safe to run
 */
static ErrorOrPtr __verify_driver_functions(struct loader_ctx* ctx, bool verify_manifest)
{
  size_t function_count;
  struct elf64_shdr* symtab_shdr;
  struct elf64_sym* sym_table_start;
  dev_manifest_t* manifest = ctx->driver->m_manifest;

  void* driver_functions[] = {
    manifest->m_handle->f_init,
    manifest->m_handle->f_exit,
    manifest->m_handle->f_probe,
    manifest->m_handle->f_msg,
  };

  void* manifest_functions[] = {
    /* These functions may be set by the driver once it inits, so we should check these again after initialization */
    manifest->m_ops.f_read,
    manifest->m_ops.f_write,
  };

  void** functions = (verify_manifest ? manifest_functions : driver_functions);

  if (verify_manifest)
    function_count = sizeof(manifest_functions) / sizeof(*manifest_functions);
  else
    function_count = sizeof(driver_functions) / sizeof(*driver_functions);

  /* Grab the correct sections */
  symtab_shdr = &ctx->shdrs[ctx->symtab_idx];
  sym_table_start = (struct elf64_sym*)symtab_shdr->sh_addr;
  
  for (uintptr_t i = 0; i < function_count; i++) {
    bool found_symbol = false;
    void* func = functions[i];

    /* Skip functions that are NULL */
    if (!func)
      continue;

    /* If the function isn't mapped high, it's just instantly invalid lol */
    if (func <= (void*)HIGH_MAP_BASE)
      return Error();

    for (uintptr_t j = 0; j < ctx->sym_count; j++) {
      struct elf64_sym* current_symbol = &sym_table_start[j];

      if (current_symbol->st_value == (uintptr_t)func) {
        found_symbol = true;
        break;
      }
    }
    if (!found_symbol)
      return Error();
  }

  return Success(0);
}

/*
 * Initializes the internal driver structures and installs
 * the driver in the kernel context. When the driver is put
 * into place, load_driver will call its init function
 */
static ErrorOrPtr __init_driver(struct loader_ctx* ctx)
{
  ErrorOrPtr result;
  struct elf64_shdr* driver_header;
  aniva_driver_t* driver_data;

  driver_header = &ctx->shdrs[ctx->expdrv_idx];
  driver_data = (aniva_driver_t*)driver_header->sh_addr;

  result = manifest_emplace_handle(ctx->driver->m_manifest, driver_data);

  if (IsError(result))
    return Error();

  result = __verify_driver_functions(ctx, false);

  if (IsError(result))
    return Error();

  __detect_driver_attributes(ctx->driver->m_manifest);

  manifest_gather_dependencies(ctx->driver->m_manifest);

  /* We could install the driver, so we came that far =D */
  ctx->driver->m_manifest->m_driver_file_path = ctx->path;
  ctx->driver->m_manifest->m_private = ctx->driver;

  return load_driver(ctx->driver->m_manifest);
}

/*
 * TODO: match every allocation with a deallocation on load failure
 */
static ErrorOrPtr __load_ext_driver(struct loader_ctx* ctx)
{
  int error;

  /* TODO: implement + check signatures */
  error = __check_driver(ctx);

  if (error)
    return Error();

  if (IsError(__move_driver(ctx)))
    return Error();

  return __init_driver(ctx);
}

/*
 * @brief load an external driver from a file
 *
 * This will load, verify and init an external driver/klib from
 * a file. This will ultimately fail if the driver is already installed
 */
extern_driver_t* load_external_driver(const char* path)
{
  int error;
  ErrorOrPtr result;
  uintptr_t driver_load_base;
  size_t read_size;
  vobj_t* file_obj;
  file_t* file;
  extern_driver_t* out;
  struct loader_ctx ctx = { 0 };

  file_obj = vfs_resolve(path);
  file = vobj_get_file(file_obj);

  if (!file)
    return nullptr;

  out = create_external_driver(NULL);

  if (!out || !out->m_manifest)
    return nullptr;

  /* Make sure we know this is external */
  out->m_manifest->m_flags |= DRV_IS_EXTERNAL;

  /* Allocate contiguous space for the driver */
  result = __kmem_alloc_range(nullptr, out->m_manifest->m_resources, HIGH_MAP_BASE, file->m_buffer_size, NULL, NULL);

  if (IsError(result))
    goto fail_and_deallocate;

  driver_load_base = Release(result);

  /* Set driver fields */
  out->m_load_base = driver_load_base;
  out->m_load_size = ALIGN_UP(file->m_buffer_size, SMALL_PAGE_SIZE);
  out->m_file = file;

  read_size = file->m_buffer_size;

  /* Read the driver into RAM */
  error = file->m_ops->f_read(file, (void*)out->m_load_base, &read_size, 0);

  if (error < 0)
    goto fail_and_deallocate;

  ctx.hdr = (struct elf64_hdr*)driver_load_base;
  ctx.size = read_size;
  ctx.driver = out;
  ctx.path = path;

  result = __load_ext_driver(&ctx);

  if (IsError(result))
    goto fail_and_deallocate;

  return out;

fail_and_deallocate:
  if (out)
    destroy_external_driver(out);

  /* TODO */
  return nullptr;
}

/*!
 * @brief Deals with the driver memory and resources
 *
 * Called from unload_driver after the ->f_exit function has 
 * been called. At this point we can deallocate the driver and stuff
 *
 * The driver manifest will still be installed in the driver tree, so 
 * TODO: when we try to load an external driver that was already installed,
 * we can reuse that manifest. When the driver is also uninstalled and after 
 * that it is loaded AGAIN, we have to create a new manifest for it
 */
void unload_external_driver(extern_driver_t* driver)
{
  if (!driver)
    return;

  /*
   * the destructor of the external driver will deallocate most if not all
   * of the resources used by the driver. This will happen since the driver gets assigned a resource
   * context, which gets entered every time the driver is called. Every subsequent allocation will go through
   * the context and gets tracked, so that we can clean it up nicely when the context is destroyed
   */
  destroy_external_driver(driver);
}

/*
 * TODO: in order to prob whether there is a driver present in a file, there must be a few
 * conditions that are met:
 *
 * o the file is a valid ELF
 * o the ELF contains a valid (TODO) signature
 * o the ELF contains the .expdrv section which is sizeof(aniva_driver_t)
 *
 * This requires us to load the entire file however, so this is kinda kostly =/
 */
bool file_contains_driver(file_t* file)
{
  (void)file;
  return false;
}