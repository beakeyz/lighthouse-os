/*
 * ELF routines (relocations, sym resolving, ect.)
 *
 * NOTE: Most ELF loading procedures in this files will be run inside the addresspace of the proc inside elf_image_t. This
 * means we don't need to do weird kernel address lookups
 */
#include "libk/bin/elf.h"
#include <libk/math/math.h>
#include "drivers/env/dynldr/priv.h"
#include "fs/file.h"
#include "libk/bin/elf_types.h"
#include "libk/data/hashmap.h"
#include "libk/data/linkedlist.h"
#include "libk/flow/error.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem.h"
#include "mem/tracker/tracker.h"
#include "mem/zalloc/zalloc.h"
#include "proc/core.h"
#include "proc/proc.h"
#include <lightos/lightos.h>

/*!
 * @brief: Load static information of an elf image
 *
 */
kerror_t load_elf_image(elf_image_t* image, proc_t* proc, file_t* file)
{
    memset(image, 0, sizeof(*image));

    image->proc = proc;
    image->kernel_image_size = ALIGN_UP(file->m_total_size, SMALL_PAGE_SIZE);

    /* Allocate space for the image */
    ASSERT(kmem_kernel_alloc_range(&image->kernel_image, image->kernel_image_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE) == 0);

    if (!file_read(file, image->kernel_image, file->m_total_size, 0))
        return -KERR_IO;

    image->elf_hdr = image->kernel_image;

    if (!elf_verify_header(image->elf_hdr))
        goto error_and_exit;

    image->elf_phdrs = elf_load_phdrs_64(file, image->elf_hdr);

    return 0;
error_and_exit:
    kmem_kernel_dealloc((vaddr_t)image->kernel_image, image->kernel_image_size);

    return -KERR_INVAL;
}

/*!
 * @brief: Murder
 */
void destroy_elf_image(elf_image_t* image)
{
    kmem_kernel_dealloc((vaddr_t)image->kernel_image, image->kernel_image_size);

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
    size_t user_low;
    page_range_t range;
    struct elf64_hdr* hdr;
    struct elf64_phdr* phdr_list;
    struct elf64_phdr* c_phdr;

    proc = image->proc;
    hdr = image->elf_hdr;
    phdr_list = image->elf_phdrs;

    user_high = NULL;
    user_low = (u64)-1;

    /* First, we calculate the size of this image */
    for (uint32_t i = 0; i < hdr->e_phnum; i++) {
        c_phdr = &phdr_list[i];

        switch (c_phdr->p_type) {
        case PT_LOAD:
            if (c_phdr->p_vaddr < user_low)
                user_low = c_phdr->p_vaddr;
            /* Find the high end */
            if (c_phdr->p_memsz + c_phdr->p_vaddr > user_high)
                user_high = c_phdr->p_memsz + c_phdr->p_vaddr;
            break;
        }
    }

    /* Would be weird if we have a reverse image or sm like that? */
    ASSERT(user_high > user_low);

    /* Simple delta */
    image->user_image_size = ALIGN_UP(user_high - user_low, SMALL_PAGE_SIZE);

    /* With this we can find the user base */
    ASSERT(page_tracker_find_fitting_range(&image->proc->m_virtual_tracker, NULL, kmem_get_page_idx(image->user_image_size), &range) == 0);

    /* Get the pointer */
    image->user_base = (void*)page_range_to_ptr(&range);

    //page_tracker_dump(&image->proc->m_virtual_tracker);

    KLOG_DBG("Found elf image user base: 0x%p (size=%lld bytes)\n", image->user_base, image->user_image_size);

    /* Now we can actually start to load the headers */
    for (uint32_t i = 0; i < hdr->e_phnum; i++) {
        c_phdr = &phdr_list[i];

        switch (c_phdr->p_type) {
        case PT_LOAD: {
            vaddr_t v_user_phdr_start;
            vaddr_t virtual_phdr_base = (vaddr_t)image->user_base + c_phdr->p_vaddr;
            size_t phdr_size = c_phdr->p_memsz;

            ASSERT(kmem_user_alloc_scattered(
                       (void**)&v_user_phdr_start,
                       proc,
                       virtual_phdr_base,
                       phdr_size,
                       KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER | KMEM_CUSTOMFLAG_NO_REMAP,
                       KMEM_FLAG_WRITABLE)
                == 0);

            //KLOG_DBG("Allocated %lld bytes at 0x%llx (base=0x%llx)\n", phdr_size, v_user_phdr_start, c_phdr->p_vaddr);

            /* Then, zero the rest of the buffer */
            /* TODO: ??? */
            memset((void*)(v_user_phdr_start), 0, phdr_size);

            /*
             * Copy elf into the mapped area
             */
            memcpy((void*)v_user_phdr_start, image->kernel_image + c_phdr->p_offset,
                MIN(phdr_size, c_phdr->p_filesz));
        } break;
        case PT_DYNAMIC:
            /* Remap to the kernel */
            image->elf_dyntbl_mapsize = ALIGN_UP(c_phdr->p_memsz, SMALL_PAGE_SIZE);
            image->elf_dyntbl = image->user_base + c_phdr->p_vaddr;
            //ASSERT(kmem_get_kernel_address((vaddr_t*)&image->elf_dyntbl, (vaddr_t)image->user_base + c_phdr->p_vaddr, proc->m_root_pd.m_root) == 0);

            // printf("Setting PT_DYNAMIC addr=0x%p size=0x%llx\n", image->elf_dyntbl, image->elf_dyntbl_mapsize);
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
    struct elf64_shdr* shdr;

    /* First pass: Fix all the addresses of the section headers */
    for (uint32_t i = 0; i < image->elf_hdr->e_shnum; i++) {
        shdr = elf_get_shdr(image->elf_hdr, i);

        if (!shdr->sh_addr)
            continue;

        /* Add this thing */
        shdr->sh_addr += (uint64_t)image->user_base;

        switch (shdr->sh_type) {
        case SHT_NOBITS:
            if (!shdr->sh_size)
                break;

            /* NOBITS sections should be included in the pheaders. Just clear them */
            memset((void*)shdr->sh_addr, 0, shdr->sh_size);
            break;
        }
    }

    image->elf_symtbl_hdr = nullptr;
    image->elf_strtbl_hdr = nullptr;
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
                image->elf_strtbl_hdr = shdr;

            break;
        case SHT_PROGBITS:
            if (strncmp(LIGHTENTRY_SECTION_NAME, image->elf_shstrtab + shdr->sh_name, sizeof(LIGHTENTRY_SECTION_NAME) - 1) == 0) {
                image->elf_lightentry_hdr = shdr;
            } else if (strncmp(LIGHTEXIT_SECTION_NAME, image->elf_shstrtab + shdr->sh_name, sizeof(LIGHTEXIT_SECTION_NAME) - 1) == 0) {
                image->elf_lightexit_hdr = shdr;
            }

            break;
        }
    }

    /* There must be at least a generic '.strtab' shdr in every ELF file */
    ASSERT_MSG(image->elf_strtbl_hdr, "dynldr: Could not find .strtab");

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
        const u32 type = ELF64_ST_TYPE(current_symbol->st_info);
        const char* sym_name = (const char*)((uint64_t)strtab + current_symbol->st_name);

        /* Debug print lmao */
        //KLOG_DBG("Found a symbol (\'%s\'). addr=0x%llx (user_base=0x%p)\n", sym_name, current_symbol->st_value, image->user_base);

        current_symbol->st_value += (vaddr_t)image->user_base;

        switch (current_symbol->st_shndx) {
        case SHN_UNDEF:
            // printf("Resolving: %s\n", sym_name);

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

            if (type >= STT_FILE)
                break;

            /* Allocate the symbol manually. This gets freed when the loaded_app is destroyed */
            sym = kzalloc(sizeof(loaded_sym_t));

            if (!sym)
                return -KERR_NOMEM;

            memset(sym, 0, sizeof(*sym));

            sym->name = sym_name;
            sym->uaddr = current_symbol->st_value;

            if (type == STT_FUNC)
                sym->flags |= LDSYM_FLAG_EXPORT;

            //KLOG_DBG("Found symbol: %s (address=0x%llx)\n", sym_name, sym->uaddr);

            /* Add both to the symbol map for easy lookup and the list for easy cleanup */
            hashmap_put(symmap, (hashmap_key_t)sym_name, sym);
            list_append(symlist, sym);
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
kerror_t _elf_do_symbols(list_t* symbol_list, hashmap_t* exported_symbol_map, loaded_app_t* app, elf_image_t* image)
{
    const char* names;
    size_t symcount;
    struct elf64_sym* syms;

    /* NOTE: Since we're working with the global symbol table (And not the dynamic symbol table)
       we need to use the coresponding GLOBAL string table */
    symcount = image->elf_symtbl_hdr->sh_size / sizeof(struct elf64_sym);
    syms = (struct elf64_sym*)_elf_get_shdr_kaddr(image, image->elf_symtbl_hdr);
    names = (const char*)_elf_get_shdr_kaddr(image, image->elf_strtbl_hdr);

    if (syms && __elf_parse_symbol_table(app, symbol_list, exported_symbol_map, image, syms, symcount, names))
        return -KERR_INVAL;

    names = image->elf_dynstrtab;
    syms = image->elf_dynsym;
    symcount = image->elf_dynsym_tblsz / sizeof(struct elf64_sym);

    /* Parse dynamic symbol table */
    if (syms && __elf_parse_symbol_table(app, symbol_list, exported_symbol_map, image, syms, symcount, names))
        return -KERR_INVAL;

    /* Now grab copy relocations
       This loop looks ugly as hell, but just deal with it sucker */
    for (uint64_t i = 0; i < image->elf_hdr->e_shnum; i++) {
        struct elf64_shdr* shdr = elf_get_shdr(image->elf_hdr, i);
        struct elf64_rel* rel;
        struct elf64_rela* rela;
        uint32_t symbol;
        size_t entry_count;
        char* symname;
        struct elf64_sym* sym;
        loaded_sym_t* loaded_sym;

        switch (shdr->sh_type) {
        case SHT_REL:
            rel = (struct elf64_rel*)shdr->sh_addr;

            /* Calculate the table size */
            entry_count = shdr->sh_size / sizeof(*rel);

            for (uint64_t i = 0; i < entry_count; i++) {
                if (ELF64_R_TYPE(rel[i].r_info) != R_X86_64_COPY)
                    continue;

                symbol = ELF64_R_SYM(rel[i].r_info);
                sym = &image->elf_dynsym[symbol];
                symname = (char*)((uintptr_t)image->elf_dynstrtab + sym->st_name);

                loaded_sym = hashmap_get(exported_symbol_map, symname);

                if (!loaded_sym)
                    break;

                loaded_sym->offset = rel[i].r_offset;
                loaded_sym->flags |= LDSYM_FLAG_IS_CPY;
            }
            break;
        case SHT_RELA:
            rela = (struct elf64_rela*)shdr->sh_addr;

            /* Calculate the table size */
            entry_count = shdr->sh_size / sizeof(*rela);

            for (uint64_t i = 0; i < entry_count; i++) {
                if (ELF64_R_TYPE(rela[i].r_info) != R_X86_64_COPY)
                    continue;

                symbol = ELF64_R_SYM(rela[i].r_info);
                sym = &image->elf_dynsym[symbol];
                symname = (char*)((uintptr_t)image->elf_dynstrtab + sym->st_name);

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
    Elf64_Word* _elf_dynsym_count_p;
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
            //ASSERT(kmem_get_kernel_address((vaddr_t*)&strtab, (vaddr_t)image->user_base + dyns_entry->d_un.d_ptr, image->proc->m_root_pd.m_root) == 0);
            strtab = image->user_base + dyns_entry->d_un.d_ptr;
            break;
        case DT_SYMTAB:
            //ASSERT(kmem_get_kernel_address((vaddr_t*)&dyn_syms, (vaddr_t)image->user_base + dyns_entry->d_un.d_ptr, image->proc->m_root_pd.m_root) == 0);
            dyn_syms = image->user_base + dyns_entry->d_un.d_ptr;
            break;
        case DT_HASH:
            // image->elf_dynsym_count = ((Elf64_Word*)(Must(kmem_get_kernel_address((vaddr_t)image->user_base + dyns_entry->d_un.d_ptr, image->proc->m_root_pd.m_root))))[1];
            /* Get the address */
            //ASSERT(kmem_get_kernel_address((uint64_t*)&_elf_dynsym_count_p, (vaddr_t)image->user_base + dyns_entry->d_un.d_ptr, image->proc->m_root_pd.m_root) == 0);
            _elf_dynsym_count_p = image->user_base + dyns_entry->d_un.d_ptr;

            /* Yoink the value */
            image->elf_dynsym_tblsz = _elf_dynsym_count_p[1];
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

    // printf("Image elf dynstrtab: %p\n", strtab);
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

        if (!KERR_OK(error))
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
            vaddr_t P = (vaddr_t)image->user_base + current->r_offset;

            /* Yoink it from the kernel address */
            //ASSERT(kmem_get_kernel_address(&P, (vaddr_t)image->user_base + current->r_offset, image->proc->m_root_pd.m_root) == 0);

            /* The value of the symbol we are referring to */
            vaddr_t A = current->r_addend;

            size_t size = 0;
            vaddr_t val = 0x00;

            /* Try to get this symbol */
            c_ldsym = loaded_app_find_symbol(app, (hashmap_key_t)symname);

            if (c_ldsym)
                val = c_ldsym->uaddr;

            //KLOG_DBG("Need to relocate symbol %s (addr=0x%llx, found=%s)\n", symname, val, (c_ldsym == nullptr ? "false" : "true"));

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

