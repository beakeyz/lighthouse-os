#include "loader.h"
#include "dev/debug/serial.h"
#include "dev/external.h"
#include "dev/manifest.h"
#include "fs/vobj.h"
#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/bin/ksyms.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include <fs/vfs.h>

struct loader_ctx {
  struct elf64_hdr* hdr; 
  size_t size;
  struct elf64_shdr* shdrs;
  char* section_strings;
  char* strtab;
  extern_driver_t* driver;

  uint32_t expdrv_idx;
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
          TRY(result, __kmem_alloc_range(nullptr, ctx->driver->m_resources, HIGH_MAP_BASE, shdr->sh_size, NULL, NULL));

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

    println(to_string(rela_count));

    for (uint32_t i = 0; i < rela_count; i++) {
      struct elf64_rela* current = &table[i];

      println(to_string(i));

      /* Where we are going to change stuff */
      vaddr_t P = target_shdr->sh_addr + current->r_offset;

      /* The value of the symbol we are referring to */
      vaddr_t S = symtable_start[ELF64_R_SYM(table[i].r_info)].st_value;
      vaddr_t A = current->r_addend;

      size_t size = 0;
      vaddr_t val = S + A;

      print("reloc type: ");
      println(to_string(ELF64_R_TYPE(current->r_info)));

      print("Target: ");
      println(elf_get_shdr_name(ctx->hdr, target_shdr));
      print("Current: ");
      println(elf_get_shdr_name(ctx->hdr, shdr));
      print("P value: ");
      println(to_string(*(uintptr_t*)P));
      print("P: ");
      println(to_string(P));
      print("S: ");
      println(to_string(S));
      print("A: ");
      println(to_string(A));

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
        case 25:
        case 29:
        case 31:
          println("Unsupported reallocation!");
          size = 8;
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
  for (uint32_t i = 0; i < ctx->hdr->e_shnum; i++) {
    struct elf64_shdr* shdr = elf_get_shdr(ctx->hdr, i);

    if (shdr->sh_type != SHT_SYMTAB)
      continue;

    /* Grab the symbol count */
    const size_t sym_count = ALIGN_UP(shdr->sh_size, sizeof(struct elf64_sym)) / sizeof(struct elf64_sym);

    /* Grab the start of the symbol name table */
    char* names = (char*)elf_get_shdr(ctx->hdr, shdr->sh_link)->sh_addr;

    /* Grab the start of the symbol table */
    struct elf64_sym* sym_table_start = (struct elf64_sym*)shdr->sh_addr;

    /* Walk the table and resolve any symbols */
    for (uint32_t i = 0; i < sym_count; i++) {
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
  }

  return Success(0);
}

static ErrorOrPtr __move_driver(struct loader_ctx* ctx)
{

  /* First, make sure any values and addresses are mapped correctly */
  if (IsError(__fixup_section_headers(ctx)))
    return Error();


  println("resolving stuff");
  if (IsError(__resolve_kernel_symbols(ctx)))
    return Error();


  println("Doing relocations r sm");
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
  uint32_t kpcdrv_sections = 0;
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
      default:

        /* Validate section */

        if (strcmp(ctx->section_strings + shdr->sh_name, ".expdrv") == 0) {
          /* TODO: real validation */

          if (shdr->sh_size != sizeof(aniva_driver_t))
            return -1;

          kpcdrv_sections++;
          ctx->expdrv_idx = i;
        }
        break;
    }
  }

  if (kpcdrv_sections != 1)
    return -1;

  return 0;
}

/*
 * TODO: match every allocation with a deallocation on load failure
 */
static ErrorOrPtr __load_ext_driver(struct loader_ctx* ctx)
{

  int error;

  error = __check_driver(ctx);

  if (error)
    return Error();

  if (IsError(__move_driver(ctx)))
    return Error();

  struct elf64_shdr* driver_header = &ctx->shdrs[ctx->expdrv_idx];
  aniva_driver_t* driver = (aniva_driver_t*)driver_header->sh_addr;

  println(driver->m_name);

  int i = driver->f_init();

  println(to_string(i));

  kernel_panic("TODO: load external driver");

  return Success(0);
}

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

  out = create_external_driver(NULL);

  if (!out)
    return nullptr;

  file_obj = vfs_resolve(path);
  file = vobj_get_file(file_obj);

  if (!file)
    goto fail_and_deallocate;

  /* Allocate contiguous space for the driver */
  result = __kmem_alloc_range(nullptr, out->m_resources, HIGH_MAP_BASE, file->m_buffer_size, NULL, NULL);

  if (IsError(result))
    goto fail_and_deallocate;

  driver_load_base = Release(result);

  /* Set driver fields */
  out->m_load_base = driver_load_base;
  out->m_load_size = ALIGN_UP(file->m_buffer_size, SMALL_PAGE_SIZE);

  read_size = file->m_buffer_size;

  /* Read the driver into RAM */
  error = file->m_ops->f_read(file, (void*)out->m_load_base, &read_size, 0);

  if (error < 0)
    goto fail_and_deallocate;

  ctx.hdr = (struct elf64_hdr*)driver_load_base;
  ctx.size = read_size;
  ctx.driver = out;

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


bool file_contains_drivers(file_t* file)
{
  return false;
}
