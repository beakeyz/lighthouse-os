/*
 * ELF routines (relocations, sym resolving, ect.)
 */
#include "libk/bin/elf.h"
#include "drivers/env/dynldr/priv.h"
#include "fs/file.h"
#include "libk/bin/elf_types.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "lightos/lib/lightos.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc/zalloc.h"
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

  kfree(image->elf_phdrs);
}

static inline void* _elf_get_shdr_kaddr(elf_image_t* image, struct elf64_shdr* hdr)
{
  if (!image)
    return nullptr;

  return image->kernel_image + hdr->sh_offset;
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
  struct elf64_hdr* hdr;
  struct elf64_phdr* phdr_list;
  struct elf64_phdr* c_phdr;

  proc = image->proc;
  hdr = image->elf_hdr;
  phdr_list = image->elf_phdrs;

  user_high = NULL;

  /* First, we calculate the size of this image */
  for (uint32_t i = 0; i < hdr->e_phnum; i++) {
    c_phdr = &phdr_list[i];

    switch (c_phdr->p_type) {
      case PT_LOAD:
        /* Find the high end */
        if (c_phdr->p_memsz + c_phdr->p_vaddr > user_high)
            user_high = c_phdr->p_memsz + c_phdr->p_vaddr;
        break;
    }
  }

  /* Simple delta */
  image->user_image_size = ALIGN_UP(user_high, SMALL_PAGE_SIZE);
  /* With this we can find the user base */
  image->user_base = (void*)(ALIGN_DOWN(Must(resource_find_usable_range(image->proc->m_resource_bundle, KRES_TYPE_MEM, image->user_image_size)), SMALL_PAGE_SIZE));

  //printf("Found elf image user base: 0x%p (size=%lld bytes)\n", image->user_base, image->user_image_size);

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

          //printf("Allocated %lld bytes at 0x%llx (kernel_addr=%llx)\n", phdr_size, v_user_phdr_start, v_kernel_phdr_start);

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
        image->elf_dyntbl = (void*)Must(kmem_get_kernel_address((vaddr_t)image->user_base + c_phdr->p_vaddr, proc->m_root_pd.m_root));

        //printf("Setting PT_DYNAMIC addr=0x%p size=0x%llx\n", image->elf_dyntbl, image->elf_dyntbl_mapsize);
        break;
    }
  }

  /* Suck my dick */
  return KERR_NONE;
}

/*!
 * @brief: Do preperations on the ELF section headers
 *
 * 1) Fix header offset and ensure backing ram
 * 2) Cache certain interesting tables
 */
kerror_t _elf_do_headers(elf_image_t* image)
{
  void* kaddr;
  struct elf64_shdr* shdr;

  /* First pass: Fix all the addresses of the section headers */
  for (uint32_t i = 0; i < image->elf_hdr->e_shnum; i++) {
    shdr = elf_get_shdr(image->elf_hdr, i);

    switch (shdr->sh_type) {
      case SHT_NOBITS:
        if (!shdr->sh_size)
          break;

        /* Just give any NOBITS sections a bit of memory */
        shdr->sh_addr = Must(kmem_user_alloc_range(image->proc, shdr->sh_size, NULL, KMEM_FLAG_WRITABLE));

        /* Zero the scattered range (FIXME: How do we assure that we don't reach memory we haven't mapped to the kernel?) */
        for (uint64_t i = 0; i < GET_PAGECOUNT(shdr->sh_addr, shdr->sh_size); i++) {
          kaddr = (void*)Must(kmem_get_kernel_address(shdr->sh_addr + kmem_get_page_addr(i), image->proc->m_root_pd.m_root));

          memset(kaddr, 0, SMALL_PAGE_SIZE);
        }
        break;
      default:
        shdr->sh_addr += (uint64_t)image->user_base;
        break;
    }
  }

  image->elf_symtbl_hdr = nullptr;
  /* Cache the section string table */
  image->elf_shstrtab = _elf_get_shdr_kaddr(image, elf_get_shdr(image->elf_hdr, image->elf_hdr->e_shstrndx));

  /* Second pass: Find the symbol table
   * FIXME: Do we need to check the name of this shared header? */
  for (uint32_t i = 0; i < image->elf_hdr->e_shnum; i++) {
    struct elf64_shdr* shdr = elf_get_shdr(image->elf_hdr, i);

    switch (shdr->sh_type) {
      case SHT_SYMTAB:
        /* Idk if there can be more than one, but in most cases there should be only one of these guys. */
        if (!image->elf_symtbl_hdr)
          image->elf_symtbl_hdr = shdr;
        break;
      case SHT_STRTAB:
        if (strncmp(".strtab", image->elf_shstrtab + shdr->sh_name, strlen(".strtab")) == 0)
          image->elf_strtab = _elf_get_shdr_kaddr(image, shdr);

        break;
      case SHT_PROGBITS:
        if (strncmp(LIGHTENTRY_SECTION_NAME, image->elf_shstrtab + shdr->sh_name, strlen(LIGHTENTRY_SECTION_NAME)) == 0)
          image->elf_lightentry_hdr = shdr;

        break;
    }
  }

  /* There must be at least a generic '.strtab' shdr in every ELF file */
  ASSERT(image->elf_strtab);

  return 0;
}

static inline kerror_t __elf_parse_symbol_table(loaded_app_t* app, list_t* symlist, hashmap_t* symmap, elf_image_t* image, struct elf64_sym* symtable, size_t sym_count, const char* strtab)
{
  loaded_sym_t* sym;

  /* Grab the start of the symbol table */
  struct elf64_sym* sym_table_start = symtable;

  /* Walk the table and resolve any symbols */
  for (uint32_t i = 0; i < sym_count; i++) {
    struct elf64_sym* current_symbol = &sym_table_start[i];
    const char* sym_name = (const char*)((uint64_t)strtab + current_symbol->st_name);

    /* Debug print lmao */
    //printf("Found a symbol (\'%s\'). info=0x%x\n", sym_name, current_symbol->st_info);

    current_symbol->st_value += (vaddr_t)image->user_base;

    switch (current_symbol->st_shndx) {
      case SHN_UNDEF:
        //printf("Resolving: %s\n", sym_name);

        /*
         * Need to look for this symbol in an earlier loaded binary =/ 
         */
        sym = loaded_app_find_symbol(app, (hashmap_key_t)sym_name);
        
        if (!sym)
          break;

        current_symbol->st_value = sym->uaddr;
        sym->usecount++;
        break;
      default:

        /* Don't add weird symbols */
        if (!strlen(sym_name))
          break;

        /* Allocate the symbol manually. This gets freed when the loaded_app is destroyed */
        sym = kzalloc(sizeof(loaded_sym_t));

        if (!sym)
          return -KERR_NOMEM;

        memset(sym, 0, sizeof (*sym));

        sym->name = sym_name;
        sym->uaddr = current_symbol->st_value;

        /* Add both to the symbol map for easy lookup and the list for easy cleanup */
        hashmap_put(symmap, (hashmap_key_t)sym_name, sym);
        list_append(symlist, sym);
        break;
    }
  }
  return 0;
}

static inline void _elf_mark_exported_symbols(loaded_app_t* app, elf_image_t* image)
{
  loaded_sym_t* sym;
  size_t sym_count;
  const char* strtab;
  struct elf64_sym* sym_table_start;

  if (!image->elf_dynsym || !image->elf_dynsym_count || !image->elf_dynstrtab)
    return;

  /* Grab the start of the symbol table */
  sym_table_start = image->elf_dynsym;
  sym_count = image->elf_dynsym_count;
  strtab = image->elf_dynstrtab;

  /* Walk the table and resolve any symbols */
  for (uint32_t i = 0; i < sym_count; i++) {
    struct elf64_sym* current_symbol = &sym_table_start[i];
    const char* sym_name = (const char*)((uint64_t)strtab + current_symbol->st_name);

    if (current_symbol->st_shndx == SHN_UNDEF)
      continue;

    /* Don't add weird symbols */
    if (!strlen(sym_name))
      continue;

    sym = loaded_app_find_symbol(app, sym_name);

    /* Mark this symbol as exported lololol */
    if (sym)
      sym->flags |= LDSYM_FLAG_EXPORT;
  }
}

/*!
 * @brief: Scan the symbols in a given elf header and grab/resolve it's symbols
 *
 * When we fail to find a symbol, this function fails and returns -KERR_INVAL
 */
kerror_t _elf_do_symbols(list_t* symbol_list, hashmap_t* exported_symbol_map, loaded_app_t* app, elf_image_t* image)
{
  const char* names;
  size_t symcount;
  struct elf64_sym* syms;

  /* NOTE: Since we're working with the global symbol table (And not the dynamic symbol table) 
     we need to use the coresponding GLOBAL string table */
  names = image->elf_strtab;
  symcount = image->elf_symtbl_hdr->sh_size / sizeof(struct elf64_sym);
  syms = (struct elf64_sym*)_elf_get_shdr_kaddr(image, image->elf_symtbl_hdr);

  if (!syms)
    return -KERR_INVAL;

  if (__elf_parse_symbol_table(app, symbol_list, exported_symbol_map, image, syms, symcount, names))
    return -KERR_INVAL;

  _elf_mark_exported_symbols(app, image);

  /* Now grab copy relocations
     This loop looks ugly as hell, but just deal with it sucker */
  for (uint64_t i = 0; i < image->elf_hdr->e_shnum; i++) {
    struct elf64_shdr* shdr = elf_get_shdr(image->elf_hdr, i);
    struct elf64_rel* rel;
    struct elf64_rela* rela;
    uint32_t symbol;
    size_t entry_count;
    char * symname;
    struct elf64_sym* sym;
    loaded_sym_t* loaded_sym;

    switch (shdr->sh_type) {
      case SHT_REL:
        rel = (struct elf64_rel*)_elf_get_shdr_kaddr(image, shdr);

        /* Calculate the table size */
        entry_count = shdr->sh_size / sizeof(*rel);

        for (uint64_t i = 0; i < entry_count; i++) {
          if (ELF64_R_TYPE(rel[i].r_info) != R_X86_64_COPY)
            continue;

          symbol  = ELF64_R_SYM(rel[i].r_info);
          sym     = &image->elf_dynsym[symbol];
          symname = (char *)((uintptr_t)image->elf_dynstrtab + sym->st_name);

          loaded_sym = hashmap_get(exported_symbol_map, symname);

          if (!loaded_sym)
            break;

          loaded_sym->offset = rel[i].r_offset;
          loaded_sym->flags |= LDSYM_FLAG_IS_CPY;

        }
        break;
      case SHT_RELA:
        rela = (struct elf64_rela*)_elf_get_shdr_kaddr(image, shdr);

        /* Calculate the table size */
        entry_count = shdr->sh_size / sizeof(*rela);

        for (uint64_t i = 0; i < entry_count; i++) {
          if (ELF64_R_TYPE(rela[i].r_info) != R_X86_64_COPY)
            continue;

          symbol  = ELF64_R_SYM(rela[i].r_info);
          sym     = &image->elf_dynsym[symbol];
          symname = (char *)((uintptr_t)image->elf_dynstrtab + sym->st_name);

          loaded_sym = hashmap_get(exported_symbol_map, symname);

          if (loaded_sym)
            break;

          loaded_sym->offset = rela[i].r_offset;
          loaded_sym->flags |= LDSYM_FLAG_IS_CPY;
        }
        break;
      default:
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
  dyns_entry = image->elf_dyntbl;

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
      case DT_HASH:
        image->elf_dynsym_count = ((Elf64_Word*)(Must(kmem_get_kernel_address((vaddr_t)image->user_base + dyns_entry->d_un.d_ptr, image->proc->m_root_pd.m_root))))[1];
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

  //printf("Image elf dynstrtab: %p\n", strtab);
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
    error = load_dynamic_lib((const char*)(image->elf_dynstrtab + dyns_entry->d_un.d_val), app, NULL);

    if (!KERR_OK(error) && kerror_is_fatal(error))
      return error;

cycle:
    dyns_entry++;
  }

  return 0;
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
kerror_t _elf_do_relocations(elf_image_t* image, loaded_app_t* app)
{
  struct elf64_hdr* hdr;
  struct elf64_shdr* shdr;
  struct elf64_rela* table;
  struct elf64_sym* symtable_start;
  loaded_sym_t* c_ldsym;
  size_t rela_count;

  hdr = image->elf_hdr;

  /* Walk the section headers */
  for (uint32_t i = 0; i < hdr->e_shnum; i++) {
    shdr = elf_get_shdr(hdr, i);

    if (shdr->sh_type != SHT_RELA)
      continue;

    /* Get the rel table of this section */
    table = (struct elf64_rela*)_elf_get_shdr_kaddr(image, shdr);

    /* Get the start of the symbol table */
    symtable_start = (struct elf64_sym*)_elf_get_shdr_kaddr(image, elf_get_shdr(hdr, shdr->sh_link));

    /* Compute the amount of relocations */
    rela_count = shdr->sh_size / sizeof(struct elf64_rela);

    for (uint32_t i = 0; i < rela_count; i++) {
      struct elf64_rela* current = &table[i];
      struct elf64_sym* sym = &symtable_start[ELF64_R_SYM(table[i].r_info)];
      const char* symname = image->elf_dynstrtab + sym->st_name;

      /* Where we are going to change stuff */
      const vaddr_t P = Must(kmem_get_kernel_address((vaddr_t)image->user_base + current->r_offset, image->proc->m_root_pd.m_root));

      /* The value of the symbol we are referring to */
      vaddr_t A = current->r_addend;

      size_t size = 0;
      vaddr_t val = 0x00;

      /* Try to get this symbol */
      c_ldsym = loaded_app_find_symbol(app, (hashmap_key_t)symname);

      if (c_ldsym)
        val = c_ldsym->uaddr;

      //printf("Need to relocate symbol %s\n", c_ldsym->name);

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
          if (c_ldsym && (c_ldsym->flags & LDSYM_FLAG_IS_CPY) == LDSYM_FLAG_IS_CPY)
            val = c_ldsym->offset;
        case R_X86_64_JUMP_SLOT:
          size = sizeof(uintptr_t);
          break;
        case R_X86_64_TPOFF64:
        default:
          size = 0;
          KLOG_ERR("Unsuported relocation type %lld!\n", ELF64_R_TYPE(current->r_info));
          kernel_panic("yikes");
          break;
      }

      if (!size)
        return -KERR_INVAL;

      memcpy((void*)P, &val, size);
    }
  }

  return KERR_NONE;
}
