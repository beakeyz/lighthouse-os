#include "loader.h"
#include "dev/core.h"
#include "dev/driver.h"
#include "dev/external.h"
#include "dev/manifest.h"
#include "fs/file.h"
#include "libk/bin/elf.h"
#include "libk/bin/elf_types.h"
#include "libk/bin/ksyms.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "system/profile/profile.h"
#include "system/resource.h"
#include "system/sysvar/var.h"

struct loader_ctx {
    const char* path;
    struct elf64_hdr* hdr;
    struct elf64_shdr* shdrs;
    char* section_strings;
    char* strtab;
    extern_driver_t* driver;
    aniva_driver_t* zig_driver;

    size_t size;
    size_t sym_count;

    uint32_t expdrv_idx, deps_idx, expsym_idx, symtab_idx;
};

/*!
 * @brief: Make sure there is backing memory for sections that need is
 */
static kerror_t __fixup_section_headers(struct loader_ctx* ctx)
{
    kerror_t error;
    void* result;

    for (uint32_t i = 0; i < ctx->hdr->e_shnum; i++) {
        struct elf64_shdr* shdr = elf_get_shdr(ctx->hdr, i);

        switch (shdr->sh_type) {
        case SHT_NOBITS: {
            if (!shdr->sh_size)
                break;

            /* Just give any NOBITS sections a bit of memory */
            error = __kmem_alloc_range(&result, nullptr, ctx->driver->m_manifest->m_resources, HIGH_MAP_BASE, shdr->sh_size, NULL, KMEM_FLAG_WRITABLE | KMEM_FLAG_KERNEL);

            if (error)
                return error;

            shdr->sh_addr = (vaddr_t)result;
            break;
        }
        default:
            shdr->sh_addr = (uintptr_t)ctx->hdr + shdr->sh_offset;
            break;
        }
    }

    return (0);
}

/*!
 * @brief: Do essensial relocations
 *
 * Simply finds relocation section headers and does ELF section relocating
 */
static kerror_t __do_driver_relocations(struct loader_ctx* ctx)
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

            KLOG_DBG("Relocation (%d/%lld) type=%lld size=0x%llx\n", i, rela_count, ELF64_R_TYPE(current->r_info), size);

            if (!size)
                return -1;

            memcpy((void*)P, &val, size);
        }
    }

    return (0);
}

static inline vaddr_t __get_symbol_address(char* name)
{
    vaddr_t ret;

    if (!name)
        return NULL;

    ret = get_ksym_address(name);

    if (ret)
        return ret;

    return get_exported_drvsym(name);
}

/*!
 * @brief: Loop over the unresolved symbols in the binary and try to match them to kernel symbols
 *
 * When we fail to find a symbol, this function fails and returns
 */
static kerror_t __resolve_kernel_symbols(struct loader_ctx* ctx)
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
            current_symbol->st_value = __get_symbol_address(sym_name);

            /* Oops, invalid symbol: bail */
            if (!current_symbol->st_value)
                break;

            ctx->driver->m_ksymbol_count++;
            break;
        default: {
            struct elf64_shdr* hdr = elf_get_shdr(ctx->hdr, current_symbol->st_shndx);
            current_symbol->st_value += hdr->sh_addr;

            /* Check if we can find a zig mofo here */
            if (strncmp(sym_name, DRVLOADER_ZIG_DRIVER_SYM, strlen(DRVLOADER_ZIG_DRIVER_SYM)) == 0 && !ctx->zig_driver) {
                ctx->zig_driver = (aniva_driver_t*)current_symbol->st_value;
                ctx->driver->m_manifest->m_flags |= DRV_ZIG;
                break;
            }

            if (!ctx->expsym_idx)
                break;

            if (ELF64_ST_TYPE(current_symbol->st_info) != STT_FUNC || current_symbol->st_shndx != ctx->expsym_idx)
                break;

            set_exported_drvsym(ctx->driver, sym_name, current_symbol->st_value);
        }
        }
    }

    return (0);
}

/*!
 * @brief: Do everything to move the driver binary in order to run it
 *
 * Does:
 *  - sheader fixup
 *  - resolves kernel symbols
 *  - relocates sections
 */
static kerror_t __move_driver(struct loader_ctx* ctx)
{
    KLOG("A\n");
    /* First, make sure any values and addresses are mapped correctly */
    if ((__fixup_section_headers(ctx)))
        return -1;

    KLOG("B\n");
    if ((__resolve_kernel_symbols(ctx)))
        return -1;

    KLOG("C\n");
    /* Then handle any pending relocations */
    if ((__do_driver_relocations(ctx)))
        return -1;

    KLOG("D\n");
    return (0);
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
             expsym_sections = 0,
             deps_sections = 0,
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
        case SHT_SYMTAB: {
            ctx->symtab_idx = i;
            symtab_sections++;
        }
        default:

            /* Validate section */
            if (strcmp(".expdrv", ctx->section_strings + shdr->sh_name) == 0) {
                /* TODO: real validation */

                if (shdr->sh_size != sizeof(aniva_driver_t)) {
                    kernel_panic("Size check failed while loading external driver");
                    return -1;
                }

                expdrv_sections++;
                ctx->expdrv_idx = i;
            } else if (strcmp(".deps", ctx->section_strings + shdr->sh_name) == 0) {
                /* TODO: real validation */
                deps_sections++;
                ctx->deps_idx = i;
            } else if (strncmp(EXPSYM_SHDR_NAME, ctx->section_strings + shdr->sh_name, strlen(EXPSYM_SHDR_NAME)) == 0) {
                expsym_sections++;
                ctx->expsym_idx = i;
            }
            break;
        }
    }

    if (symtab_sections != 1)
        return -KERR_INVAL;

    /*
     * All these sections are a must-have for external drivers so
     * they must explicitly export both a driver AND a dependency table
     */
    if (expdrv_sections != 1 || deps_sections != 1)
        return KERR_NOT_FOUND;

    /* Can't have more than one exported symbol sections */
    if (expsym_sections > 1)
        return KERR_NOT_FOUND;

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
static void __detect_driver_attributes(drv_manifest_t* manifest)
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
static kerror_t __verify_driver_functions(struct loader_ctx* ctx, bool verify_manifest)
{
    size_t function_count;
    struct elf64_shdr* symtab_shdr;
    struct elf64_sym* sym_table_start;
    drv_manifest_t* manifest = ctx->driver->m_manifest;

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
            return -1;

        for (uintptr_t j = 0; j < ctx->sym_count; j++) {
            struct elf64_sym* current_symbol = &sym_table_start[j];

            if (current_symbol->st_value == (uintptr_t)func) {
                found_symbol = true;
                break;
            }
        }
        if (!found_symbol)
            return -1;
    }

    return (0);
}

static kerror_t __remove_installed_manifest(drv_manifest_t* new_manifest)
{
    drv_manifest_t* current;

    current = get_driver(new_manifest->m_url);

    /* Driver is not yet loaded OR installed. Happy day */
    if (!current)
        return (0);

    /* This is less ideal (TODO: figure out how to safely deal with these) */
    if (is_driver_loaded(current))
        return -1;

    /* We already have an internal driver for this or something? this really should not happen */
    if (!(current->m_flags & DRV_IS_EXTERNAL))
        return 1;

    /* Remove the old driver manifest */
    return uninstall_driver(current);
}

static inline void _ctx_prepare_manifest(struct loader_ctx* ctx)
{
    if (!ctx->driver->m_manifest->m_driver_file_path)
        ctx->driver->m_manifest->m_driver_file_path = strdup(ctx->path);

    ctx->driver->m_manifest->m_external = ctx->driver;
}

/*
 * Initializes the internal driver structures and installs
 * the driver in the kernel context. When the driver is put
 * into place, load_driver will call its init function
 */
static kerror_t __init_driver(struct loader_ctx* ctx, bool install)
{
    kerror_t error;
    struct elf64_shdr* driver_header = NULL;
    struct elf64_shdr* deps_header = NULL;
    aniva_driver_t* driver_data;

    /* If we couldn't find a zig driver, use the header indeces. */
    if (!ctx->zig_driver) {
        driver_header = &ctx->shdrs[ctx->expdrv_idx];
        deps_header = &ctx->shdrs[ctx->deps_idx];

        driver_data = (aniva_driver_t*)driver_header->sh_addr;
        driver_data->m_deps = (drv_dependency_t*)deps_header->sh_addr;
    } else {
        /* Otherwise, yoink the zig driver (Can't use dependencies) */
        driver_data = ctx->zig_driver;
        driver_data->m_deps = NULL;
    }

    error = manifest_emplace_handle(ctx->driver->m_manifest, driver_data);

    if ((error))
        return -1;

    /*
     * If there already is an installed version of this driver, we need to verify
     * that it's not already loaded (and it thus in an idle installed-only state)
     * and we can thus load it here
     */
    error = __remove_installed_manifest(ctx->driver->m_manifest);

    if ((error))
        return -1;

    error = __verify_driver_functions(ctx, false);

    if ((error))
        return -1;

    __detect_driver_attributes(ctx->driver->m_manifest);

    /* When loading a new driver, we need to have valid dependencies */
    if (manifest_gather_dependencies(ctx->driver->m_manifest) < 0)
        return -1;

    if (install) {
        error = install_driver(ctx->driver->m_manifest);

        _ctx_prepare_manifest(ctx);

        return error;
    }

    _ctx_prepare_manifest(ctx);

    /* We could install the driver, so we came that far =D */
    return load_driver(ctx->driver->m_manifest);
}

/*
 * TODO: match every allocation with a deallocation on load failure
 */
static kerror_t __load_ext_driver(struct loader_ctx* ctx, bool install)
{
    int error;
    /* TODO: implement + check signatures */
    error = __check_driver(ctx);

    /* If the error is fatal, die */
    if (error < 0)
        return error;

    /* Do the relocations anyway */
    if ((__move_driver(ctx)))
        return -1;

    /* If we could find a zig driver, all is good */
    if (error && !ctx->zig_driver)
        return -1;

    return __init_driver(ctx, install);
}

/*
 * @brief load an external driver from a file
 *
 * This will load, verify and init an external driver/klib from
 * a file. This will ultimately fail if the driver is already installed
 */
extern_driver_t* load_external_driver(const char* path)
{
    kerror_t error;
    uintptr_t driver_load_base;
    size_t read_size;
    file_t* file;
    extern_driver_t* out;
    struct loader_ctx ctx = { 0 };

    file = file_open(path);

    if (!file || !file->m_total_size)
        return nullptr;

    out = create_external_driver(NULL);

    if (!out || !out->m_manifest)
        goto fail_and_deallocate;

    /* Allocate contiguous high space for the driver */
    error = __kmem_alloc_range((void**)&driver_load_base, nullptr, out->m_manifest->m_resources, HIGH_MAP_BASE, file->m_total_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE);

    if ((error))
        goto fail_and_deallocate;

    /* Set driver fields */
    out->m_load_base = driver_load_base;
    out->m_load_size = ALIGN_UP(file->m_total_size, SMALL_PAGE_SIZE);

    read_size = file_get_size(file);

    /* Read the driver into RAM */
    read_size = file_read(file, (void*)out->m_load_base, read_size, 0);

    if (!read_size)
        goto fail_and_deallocate;

    ctx.hdr = (struct elf64_hdr*)driver_load_base;
    ctx.size = read_size;
    ctx.driver = out;
    ctx.path = strdup(path);

    error = __load_ext_driver(&ctx, false);

    kfree((void*)ctx.path);

    if ((error))
        goto fail_and_deallocate;

    file_close(file);
    return out;

fail_and_deallocate:

    if (out && out->m_manifest) {
        destroy_drv_manifest(out->m_manifest);
        out->m_manifest = nullptr;
    }

    if (out)
        destroy_external_driver(out);

    if (file)
        file_close(file);

    /* TODO */
    return nullptr;
}

/*!
 * @brief: Install a driver manifest from a path
 *
 * The path should point to a file that contains driver code. We verify this and
 * we create a manifest that is only installed and not loaded. Loading this manifest
 * will error in the file being loaded from disk and the manifest being overwritten
 *
 */
drv_manifest_t* install_external_driver(const char* path)
{
    kerror_t error;
    uintptr_t driver_load_base;
    size_t read_size;
    file_t* file;
    drv_manifest_t* manifest;
    extern_driver_t* ext_drv;
    struct loader_ctx ctx = { 0 };

    manifest = nullptr;
    file = file_open(path);

    if (!file || !file->m_total_size)
        return nullptr;

    /* Create an external driver so we can  */
    ext_drv = create_external_driver(NULL);

    if (!ext_drv || !ext_drv->m_manifest)
        goto fail_and_deallocate;

    manifest = ext_drv->m_manifest;

    /* Allocate contiguous high space for the driver */
    error = __kmem_alloc_range((void**)&driver_load_base, nullptr, manifest->m_resources, HIGH_MAP_BASE, file->m_total_size, NULL, KMEM_FLAG_KERNEL | KMEM_FLAG_WRITABLE);

    read_size = file_get_size(file);

    /* Set driver fields */
    ext_drv->m_load_base = driver_load_base;
    ext_drv->m_load_size = ALIGN_UP(file->m_total_size, SMALL_PAGE_SIZE);

    /* Only check failure here (unlikely failure) */
    if ((error))
        goto fail_and_deallocate;

    /* Read the driver into RAM */
    read_size = file_read(file, (void*)driver_load_base, read_size, 0);

    if (!read_size)
        goto fail_and_deallocate;

    /* Set the context in preperation for the parse */
    ctx.hdr = (struct elf64_hdr*)driver_load_base;
    ctx.size = read_size;
    ctx.driver = ext_drv;
    ctx.path = path;

    /* Init the external driver with install-only enabled */
    error = __load_ext_driver(&ctx, true);

    if ((error))
        goto fail_and_deallocate;

    /*
     * Successfully installed the manifest, clean up stuff
     * We need to:
     *  - close the file
     *  - deallocate the driver load area
     *  - clean up weird variables in the manifest
     *
     * All of this is done by destroying the temporary ext. driver
     */
    file_close(file);

    /* Destroy the temporary external driver */
    destroy_external_driver(ext_drv);

    /* Clear the manifests resources */
    destroy_resource_bundle(manifest->m_resources);

    /* Give the manifest a clean resource bundle */
    manifest->m_resources = create_resource_bundle(NULL);

    return manifest;

fail_and_deallocate:

    if (ext_drv)
        destroy_external_driver(ext_drv);

    if (manifest)
        destroy_drv_manifest(manifest);

    if (file)
        file_close(file);

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

/*!
 * @brief: Read the driver base path from the BASE profile
 *
 * TODO: cache this value and register a profile_var change kevent
 */
static const char* _get_driver_path()
{
    const char* ret;
    sysvar_t* var;

    if (profile_find_var(DRIVERS_LOC_VAR_PATH, &var))
        return nullptr;

    if (!sysvar_get_str_value(var, &ret))
        return nullptr;

    /* Release the reference made by profile_find_var */
    release_sysvar(var);

    return ret;
}

extern_driver_t* load_external_driver_from_var(const char* varpath)
{
    kerror_t error;
    const char* driver_filename;
    const char* driver_rootpath;
    char* driver_path;
    size_t driver_path_len;
    sysvar_t* var;
    extern_driver_t* ret;

    error = profile_find_var(varpath, &var);
    if (error)
        return nullptr;

    if (!sysvar_get_str_value(var, &driver_filename))
        return nullptr;

    release_sysvar(var);

    driver_rootpath = _get_driver_path();

    if (!driver_rootpath || driver_rootpath[strlen(driver_rootpath) - 1] != '/')
        return nullptr;

    driver_path_len = strlen(driver_rootpath) + strlen(driver_filename) + 1;
    driver_path = kmalloc(driver_path_len);

    if (!driver_path)
        return nullptr;

    memset(driver_path, 0, driver_path_len);
    concat((void*)driver_rootpath, (void*)driver_filename, driver_path);

    ret = load_external_driver(driver_path);

    kfree((void*)driver_path);

    return ret;
}

/*
 * TODO: in order to prob whether there is a driver present in a file, there must be a few
 * conditions that are met:
 *
 * o the file is a valid ELF
 * o the ELF contains a valid (TODO) signature
 * o the ELF contains the .expdrv section which is sizeof(aniva_driver_t) (plus its dependency strings)
 *
 * This requires us to load the entire file however, so this is kinda kostly =/
 */
bool file_contains_driver(file_t* file)
{
    (void)file;
    return false;
}
