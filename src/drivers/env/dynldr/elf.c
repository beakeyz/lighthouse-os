
/*
 * ELF routines (relocations, sym resolving, ect.)
 */

#include "libk/bin/elf.h"
#include "drivers/env/dynldr/priv.h"
#include "fs/file.h"
#include "libk/bin/elf_types.h"
#include "libk/flow/error.h"
#include "mem/kmem_manager.h"
#include "proc/proc.h"
#include "system/resource.h"

/*!
 * @brief: Load static information of an elf image
 *
 */
kerror_t load_elf_image(elf_image_t* image, proc_t* proc, file_t* file)
{
  memset(image, 0, sizeof(*image));

  image->proc = proc;
  image->kernel_image_size = ALIGN_UP(file->m_total_size, SMALL_PAGE_SIZE);
  image->kernel_image = (void*)Must(__kmem_kernel_alloc_range(image->kernel_image_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE));

  if (!file_read(file, image->kernel_image, file->m_total_size, 0))
    return -KERR_IO;

  image->elf_hdr = image->kernel_image;

  if (!elf_verify_header(image->elf_hdr))
    goto error_and_exit;

  image->elf_phdrs = elf_load_phdrs_64(file, image->elf_hdr);

  return 0;
error_and_exit:
  __kmem_kernel_dealloc((vaddr_t)image->kernel_image, image->kernel_image_size);

  return -KERR_INVAL;
}

/*!
 * @brief: Murder
 */
void destroy_elf_image(elf_image_t* image)
{
  __kmem_kernel_dealloc((vaddr_t)image->kernel_image, image->kernel_image_size);

  if (image->elf_dyntbl)
    __kmem_kernel_dealloc((vaddr_t)image->elf_dyntbl, image->elf_dyntbl_mapsize);

  kfree(image->elf_phdrs);
}

/*!
 * @brief: Itterate the program headers and load shit we need
 *
 * This MUST be the first opperation done on an ELF image after we've got the pheaders
 * NOTE: We assume PT_INTERP is correct
 */
kerror_t _elf_load_phdrs(elf_image_t* image)
{
  proc_t* proc;
  size_t user_high;
  size_t user_low;
  struct elf64_hdr* hdr;
  struct elf64_phdr* phdr_list;
  struct elf64_phdr* c_phdr;

  proc = image->proc;
  hdr = image->elf_hdr;
  phdr_list = image->elf_phdrs;

  user_low = user_high = NULL;

  /* First, we calculate the size of this image */
  for (uint32_t i = 0; i < hdr->e_phnum; i++) {
    c_phdr = &phdr_list[i];

    switch (c_phdr->p_type) {
      case PT_LOAD:
        {
          /* Find the low and high ends */
          if (c_phdr->p_vaddr < user_low)
              user_low = c_phdr->p_vaddr;
          if (c_phdr->p_memsz + c_phdr->p_vaddr > user_high)
              user_high = c_phdr->p_memsz + c_phdr->p_vaddr;
        }
        break;
    }
  }

  /* Simple delta */
  image->user_image_size = user_high - user_low;
  /* With this we can find the user base */
  image->user_base = (void*)ALIGN_DOWN(Must(resource_find_usable_range(image->proc->m_resource_bundle, KRES_TYPE_MEM, ALIGN_UP(image->user_image_size + SMALL_PAGE_SIZE, SMALL_PAGE_SIZE))), SMALL_PAGE_SIZE);

  printf("Found elf image user base: 0x%p (size=%lld bytes)\n", image->user_base, image->user_image_size);

  /* Now we can actually start to load the headers */
  for (uint32_t i = 0; i < hdr->e_phnum; i++) {
    c_phdr = &phdr_list[i];

    switch (c_phdr->p_type) {
      case PT_LOAD:
        {
          vaddr_t virtual_phdr_base = (vaddr_t)image->user_base + c_phdr->p_vaddr;
          size_t phdr_size = c_phdr->p_memsz;

          vaddr_t v_user_phdr_start = Must(__kmem_alloc_range(
                proc->m_root_pd.m_root,
                proc->m_resource_bundle,
                virtual_phdr_base,
                phdr_size,
                KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER | KMEM_CUSTOMFLAG_NO_REMAP,
                KMEM_FLAG_WRITABLE
                ));

          /* NOTE: This gives us a memory range in HIGH_MAP_BASE, which might just fuck us over if we start
           * to allocate over 2 Gib. When we get fucked, it might be nice to try this the other way around
           * (meaning we first allocate in the kernel (With KERNEL_MAP_BASE) and then transfer the mapping to userspace)
           * or use proc_map_into_kernel. This does use a little bit more physical RAM though
           */
          vaddr_t v_kernel_phdr_start = Must(kmem_get_kernel_address(v_user_phdr_start, proc->m_root_pd.m_root));

          printf("Allocated %lld bytes at 0x%llx (kernel_addr=%llx)\n", phdr_size, v_user_phdr_start, v_kernel_phdr_start);

          /* Then, zero the rest of the buffer */
          /* TODO: ??? */
          memset((void*)(v_kernel_phdr_start), 0, phdr_size);

          /*
           * Copy elf into the mapped area 
           */
          memcpy((void*)v_kernel_phdr_start, image->kernel_image + c_phdr->p_offset, 
              c_phdr->p_filesz > c_phdr->p_memsz ?
                c_phdr->p_memsz :
                c_phdr->p_filesz
              );
        }
        break;
      case PT_DYNAMIC:
        /* Remap to the kernel */
        image->elf_dyntbl_mapsize = ALIGN_UP(c_phdr->p_memsz, SMALL_PAGE_SIZE);
        image->elf_dyntbl = proc_map_into_kernel(proc, (vaddr_t)image->user_base + c_phdr->p_vaddr, image->elf_dyntbl_mapsize);

        printf("Setting PT_DYNAMIC addr=0x%p size=0x%llx\n", image->elf_dyntbl, image->elf_dyntbl_mapsize);
        break;
    }
  }

  /* Suck my dick */
  return KERR_NONE;
}

kerror_t _elf_do_headers(elf_image_t* image)
{
  for (uint32_t i = 0; i < image->elf_hdr->e_shnum; i++) {
    struct elf64_shdr* shdr = elf_get_shdr(image->elf_hdr, i);

    switch (shdr->sh_type) {
      case SHT_SYMTAB:
        /* We also care about the default symbol header */
        image->elf_symtbl_hdr = shdr;
        break;
      case SHT_NOBITS:
        {
          if (!shdr->sh_size)
            break;

          /* Just give any NOBITS sections a bit of memory */
          shdr->sh_addr = Must(kmem_user_alloc_range(image->proc, shdr->sh_size, NULL, NULL));
          break;
        }
      default:
        shdr->sh_addr = (uint64_t)image->user_base + shdr->sh_offset;
        break;
    }
  }

  return 0;
}

/*!
 * @brief: Scan the symbols in a given elf header and grab/resolve it's symbols
 *
 * When we fail to find a symbol, this function fails and returns -KERR_INVAL
 */
kerror_t _elf_do_symbols(list_t* symbol_list, hashmap_t* exported_symbol_map, elf_image_t* image)
{
  uint32_t sym_count;
  loaded_sym_t* sym;
  struct elf64_hdr* hdr;
  struct elf64_shdr* shdr;

  hdr = image->elf_hdr;
  shdr = image->elf_symtbl_hdr;

  if (!shdr)
    return -KERR_INVAL;

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

    /* Debug print lmao */
    printf("Found a symbol (\'%s\'). type=0x%x info=0x%x\n", sym_name, current_symbol->st_other, current_symbol->st_info);

    /* We only care about functions */
    if (current_symbol->st_other != STT_FUNC)
      continue;

    switch (current_symbol->st_shndx) {
      case SHN_UNDEF:
        /*
         * Need to look for this symbol in an earlier loaded binary =/ 
         * TODO: Create a load context that keeps track of symbol addresses, origins, ect.
         */
        printf("%s\n", sym_name);
        kernel_panic("TODO: found an unresolved symbol!");

        sym = hashmap_get(exported_symbol_map, sym_name);
        
        if (!sym) 
          break;

        current_symbol->st_value = sym->uaddr;
        sym->usecount++;
        break;
      default:
        {
          /* We assume headers have already been fixed */
          struct elf64_shdr* hdr = elf_get_shdr(image->elf_hdr, current_symbol->st_shndx);
          current_symbol->st_value += hdr->sh_addr;
        }
        break;
    }
  }

  return 0;
}

static kerror_t __elf_get_dynsect_info(elf_image_t* image)
{
  char* strtab;
  struct elf64_sym* dyn_syms;
  struct elf64_dyn* dyns_entry;

  strtab = nullptr;
  dyn_syms = nullptr;
  dyns_entry = (struct elf64_dyn*)Must(kmem_get_kernel_address((vaddr_t)image->elf_dyntbl, image->proc->m_root_pd.m_root));

  /* The dynamic section table is null-terminated xD */
  while (dyns_entry->d_tag) {

    /* First look for anything that isn't the DT_NEEDED type */
    switch (dyns_entry->d_tag) {
      case DT_STRTAB:
        /* We can do this, since we have assured a kernel mapping through loading this pheader beforehand */
        strtab = (char*)Must(kmem_get_kernel_address((vaddr_t)image->user_base + dyns_entry->d_un.d_ptr, image->proc->m_root_pd.m_root));
        break;
      case DT_SYMTAB:
        dyn_syms = (struct elf64_sym*)Must(kmem_get_kernel_address((vaddr_t)image->user_base + dyns_entry->d_un.d_ptr, image->proc->m_root_pd.m_root));
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

  image->elf_dynsym = dyn_syms;
  /* This would be the stringtable for dynamic stuff */
  image->elf_dynstrtab = strtab;
  return KERR_NONE;
}

static kerror_t __elf_process_dynsect_info(elf_image_t* image, loaded_app_t* app)
{
  kerror_t error;
  struct elf64_dyn* dyns_entry;

  /* Reset the itterator */
  dyns_entry = image->elf_dyntbl;

  while (dyns_entry->d_tag) {

    if (dyns_entry->d_tag != DT_NEEDED)
      goto cycle;

    /* Load the library */
    error = load_dynamic_lib((const char*)(image->elf_strtab + dyns_entry->d_un.d_val), app);

    if (!KERR_OK(error) && kerror_is_fatal(error))
      break;

cycle:
    dyns_entry++;
  }

  return error;
}

/*!
 * @brief: Cache info about the dynamic sections
 *
 * Loop over the dynamic sections.
 * This function has two stages:
 * 1) Collect information about what is actually inside the dynamic section header
 * 2) act on the data that's inside (Load aditional libraries, fix offsets, ect.)
 */
kerror_t _elf_load_dyn_sections(elf_image_t* image, loaded_app_t* app)
{
  kerror_t error;

  if (!image)
    return -KERR_INVAL;

  /* Gather info on the dynamic part of this image */
  error = __elf_get_dynsect_info(image);

  if (error)
    return error;

  /* Do meaningful shit */
  return __elf_process_dynsect_info(image, app);
}

/*!
 * @brief: Do essensial relocations
 *
 * Simply finds relocation section headers and does ELF section relocating.
 * 
 * NOTE: This assumes to be called with @hdr being at the start of the image buffer. This means 
 * we assume that the entire binary file is loaded into a buffer, with hdr = image_base
 */
kerror_t _elf_do_relocations(elf_image_t* image)
{
  proc_t* proc;
  struct elf64_hdr* hdr;
  struct elf64_shdr* shdr;
  struct elf64_rela* table;
  struct elf64_shdr* target_shdr;
  struct elf64_sym* symtable_start;
  pml_entry_t* pml_root;
  size_t rela_count;

  proc = image->proc;
  hdr = image->elf_hdr;

  pml_root = proc->m_root_pd.m_root;

  /* Walk the section headers */
  for (uint32_t i = 0; i < hdr->e_shnum; i++) {
    shdr = elf_get_shdr(hdr, i);

    if (shdr->sh_type != SHT_RELA)
      continue;

    /* Get the rel table of this section */
    table = (struct elf64_rela*)Must(kmem_get_kernel_address(shdr->sh_addr, pml_root));

    /* Get the target section to apply these reallocacions to */
    target_shdr = elf_get_shdr(hdr, shdr->sh_info);

    /* Get the start of the symbol table */
    symtable_start = (struct elf64_sym*)Must(kmem_get_kernel_address(elf_get_shdr(hdr, shdr->sh_link)->sh_addr, pml_root));

    /* Compute the amount of relocations */
    rela_count = ALIGN_UP(shdr->sh_size, sizeof(struct elf64_rela)) / sizeof(struct elf64_rela);

    for (uint32_t i = 0; i < rela_count; i++) {
      struct elf64_rela* current = &table[i];
      struct elf64_sym* sym = &symtable_start[ELF64_R_SYM(table[i].r_info)];

      /* Where we are going to change stuff */
      const vaddr_t P = Must(kmem_get_kernel_address(target_shdr->sh_addr + current->r_offset, proc->m_root_pd.m_root));

      /* The value of the symbol we are referring to */
      vaddr_t S = sym->st_value;
      vaddr_t A = current->r_addend;

      size_t size = 0;
      vaddr_t val = S;

      /* Copy is special snowflake lmao */
      if (ELF64_R_TYPE(current->r_info) == R_X86_64_COPY) {
        memcpy((void*)P, (void*)val, sym->st_size);
        continue;
      }

      switch (ELF64_R_TYPE(current->r_info)) {
        case R_X86_64_64:
          size = sizeof(uint64_t);
          val += A;
          break;
        case R_X86_64_32S:
        case R_X86_64_32:
          size = sizeof(uint32_t);
          val += A;
          break;
        case R_X86_64_PC32:
        case R_X86_64_PLT32:
          size = sizeof(uint32_t);
          val += A;
          val -= P;
          break;
        case R_X86_64_PC64:
          size = sizeof(uint64_t);
          val += A;
          val -= P;
          break;
        case R_X86_64_RELATIVE:
          size = sizeof(uint64_t);
          val = (uint64_t)hdr + A;
          break;
        case R_X86_64_GLOB_DAT:
        case R_X86_64_JUMP_SLOT:
          break;
        case R_X86_64_TPOFF64:
          kernel_panic("TODO: implement R_X86_64_TPOFF64 relocation type");
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
