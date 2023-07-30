#include "loader.h"
#include "dev/debug/serial.h"
#include "dev/external.h"
#include "fs/vobj.h"
#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/bin/ksyms.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include <fs/vfs.h>

static ErrorOrPtr __fixup_section_headers(extern_driver_t* driver, struct elf64_hdr* file_start)
{
  for (uint32_t i = 1; i < file_start->e_shnum; i++) {
    struct elf64_shdr* shdr = elf_get_shdr(file_start, i);

    switch (shdr->sh_type) {
      case SHT_NOBITS:
        {
          if (!shdr->sh_size)
            break;

          /* Just give any NOBITS sections a bit of memory */
          TRY(result, __kmem_alloc_range(nullptr, driver->m_resources, HIGH_MAP_BASE, shdr->sh_size, NULL, NULL));

          shdr->sh_addr = result;
          break;
        }
      default:
        shdr->sh_addr = (uintptr_t)file_start + shdr->sh_offset;
        break;
    }
  }

  return Success(0);
}

static ErrorOrPtr __do_driver_relocations(extern_driver_t* driver, struct elf64_hdr* file_start, size_t file_size)
{

  /* Walk the section headers */
  for (uint32_t i = 1; i < file_start->e_shnum; i++) {
    struct elf64_shdr* shdr = elf_get_shdr(file_start, i);

    switch (shdr->sh_type) {
      case SHT_RELA:
        {
          /* Get the rel table of this section */
          struct elf64_rela* table = (struct elf64_rela*)shdr->sh_addr;

          /* Get the target section to apply these reallocacions to */
          struct elf64_shdr* target_shdr = elf_get_shdr(file_start, shdr->sh_info);

          /* Get the start of the symbol table */
          struct elf64_sym* symtable_start = (struct elf64_sym*)elf_get_shdr(file_start, shdr->sh_link)->sh_addr;

          /* Compute the amount of relocations */
          const size_t rela_count = ALIGN_UP(shdr->sh_size, sizeof(struct elf64_rela)) / sizeof(struct elf64_rela);

          println(to_string(rela_count));

          for (uint32_t i = 0; i < rela_count; i++) {
            struct elf64_rela* current = &table[i];

            println(to_string(i));

            vaddr_t P = target_shdr->sh_addr + current->r_offset;
            vaddr_t S = symtable_start[ELF64_R_SYM(table[i].r_info)].st_value;
            vaddr_t A = table[i].r_addend;

            print("reloc type: ");
            println(to_string(ELF64_R_TYPE(current->r_info)));

            switch (ELF64_R_TYPE(current->r_info)) {
              case 1:
                (*(uintptr_t*)P) = S + A;
                break;
              case 2:
                (*(uint32_t*)P) = S + A - P;
                break;
              case 10:
                (*(uint32_t*)P) = S + A;
                break;
              case 24:
                (*(uintptr_t*)P) = S + A - P;
                break;
              case 25:
              case 29:
              case 31:
                break;
            }
          }
          break;
        }
    }
  }

  return Success(0);
}

static ErrorOrPtr __resolve_kernel_symbols(extern_driver_t* driver, struct elf64_hdr* file_start)
{
  for (uint32_t i = 1; i < file_start->e_shnum; i++) {
    struct elf64_shdr* shdr = elf_get_shdr(file_start, i);

    if (shdr->sh_type != SHT_SYMTAB)
      continue;

    /* Grab the symbol count */
    const size_t sym_count = ALIGN_UP(shdr->sh_size, sizeof(struct elf64_sym)) / sizeof(struct elf64_sym);

    /* Grab the start of the symbol name table */
    char* names = (char*)elf_get_shdr(file_start, shdr->sh_link)->sh_addr;

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

          driver->m_ksymbol_count++;
          break;
        default:
          {
            struct elf64_shdr* hdr = elf_get_shdr(file_start, current_symbol->st_shndx);
            current_symbol->st_value += hdr->sh_addr;
          }
      }
    }
  }

  return Success(0);
}

static ErrorOrPtr __move_driver(extern_driver_t* out, struct elf64_hdr* file_start, size_t size)
{

  /* First, make sure any values and addresses are mapped correctly */
  if (IsError(__fixup_section_headers(out, file_start)))
    return Error();


  println("resolving stuff");
  if (IsError(__resolve_kernel_symbols(out, file_start)))
    return Error();


  println("Doing relocations r sm");
  /* Then handle any pending relocations */
  if (IsError(__do_driver_relocations(out, file_start, size)))
    return Error();

  return Success(0);
}

bool file_contains_drivers(file_t* file)
{
  return false;
}

ErrorOrPtr load_external_driver(const char* path, extern_driver_t* out)
{
  int error;
  size_t read_size;
  vobj_t* file_obj;
  file_t* file;
  aniva_driver_t* driver_data;
  struct elf64_hdr header;

  if (!out)
    return Error();

  file_obj = vfs_resolve(path);
  file = vobj_get_file(file_obj);

  if (!file)
    return Error();

  if (IsError(elf_grab_sheaders(file, &header))) {
    return Error();
  }

  /* Allocate contiguous space for the driver */
  TRY(driver_load_base, __kmem_alloc_range(nullptr, out->m_resources, HIGH_MAP_BASE, file->m_buffer_size, NULL, NULL));

  /* Set driver fields */
  out->m_load_base = driver_load_base;
  out->m_load_size = ALIGN_UP(file->m_buffer_size, SMALL_PAGE_SIZE);

  read_size = file->m_buffer_size;

  /* Read the driver into RAM */
  error = file->m_ops->f_read(file, (void*)out->m_load_base, &read_size, 0);

  if (error < 0)
    goto fail_and_deallocate;

  if (IsError(__move_driver(out, (struct elf64_hdr*)driver_load_base, read_size)))
    goto fail_and_deallocate;

  println("Looking for kpcdrv");
  struct elf64_shdr* pcdrv_sct = elf_get_shdr((struct elf64_hdr*)driver_load_base, elf_find_section((struct elf64_hdr*)driver_load_base, ".rela.kpcdrvs"));

  println(to_string((uintptr_t)pcdrv_sct));
  println(to_string(pcdrv_sct->sh_size));
  println(to_string(pcdrv_sct->sh_addr));
  println(to_string(*(uintptr_t*)pcdrv_sct->sh_addr));

  (void)driver_data;

  kernel_panic("TODO: load external driver");

  return Success(0);

fail_and_deallocate:
  /* TODO */
  return Error();
}
