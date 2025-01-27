#include "elf.h"
#include "mem/heap.h"
#include "proc/exec/elf/dynamic.h"

int elf_read(file_t* file, void* buffer, size_t* size, uintptr_t offset)
{
    *size = file_read(file, buffer, *size, offset);

    if (!(*size))
        return -1;

    return 0;
}

/*!
 * @brief Copy the contents of an ELF into the correct user pages
 *
 * When mapping a userrange scattered, we need to first read the file into a contiguous kernel
 * region, after which we can loop over all the userpages and find their physical addresses.
 * We translate these to high kernel addresses so that we can copy the correct bytes over
 */
// static int elf_read_into_user_range(pml_entry_t* root, file_t* file, vaddr_t user_start, size_t size, uintptr_t offset);

struct elf64_phdr* elf_load_phdrs_64(file_t* elf, struct elf64_hdr* elf_header)
{
    struct elf64_phdr* ret;
    int read_res;
    size_t total_size;

    if (!elf || !elf_header)
        return nullptr;

    total_size = elf_header->e_phnum * sizeof(struct elf64_phdr);

    if (!total_size || total_size > 0x10000)
        return nullptr;

    ret = kmalloc(total_size);

    if (!ret)
        return nullptr;

    read_res = elf_read(elf, ret, &total_size, elf_header->e_phoff);

    if (read_res) {
        kfree(ret);
        return nullptr;
    }

    return ret;
}

const char* elf_get_shdr_name(struct elf64_hdr* header, struct elf64_shdr* shdr)
{
    struct elf64_shdr* strhdr = elf_get_shdr(header, header->e_shstrndx);
    return (const char*)((uintptr_t)header + strhdr->sh_offset + shdr->sh_name);
}

struct elf64_shdr* elf_get_shdr(struct elf64_hdr* header, uint32_t i)
{
    return (struct elf64_shdr*)((uintptr_t)header + header->e_shoff + header->e_shentsize * i);
}

void* elf_section_start_addr(struct elf64_hdr* header, const char* name)
{
    return (void*)elf_get_shdr(header, elf_find_section(header, name))->sh_addr;
}

void* elf_section_start_size(struct elf64_hdr* header, const char* name)
{
    return (void*)elf_get_shdr(header, elf_find_section(header, name))->sh_size;
}

uint32_t elf_find_section(struct elf64_hdr* header, const char* name)
{
    const char* str_start = (const char*)((uintptr_t)header + header->e_shstrndx);

    for (uint32_t i = 1; i < header->e_shnum; i++) {
        struct elf64_shdr* shdr = elf_get_shdr(header, i);

        if (strncmp(name, str_start + shdr->sh_name, strlen(name)) == 0)
            return i;
    }

    return 0;
}

bool elf_verify_header(struct elf64_hdr* header)
{

    /* No elf? */
    if (!memcmp(header->e_ident, ELF_MAGIC, ELF_MAGIC_LEN))
        return false;

    /* No 64 bit binary =( */
    if (header->e_ident[EI_CLASS] != ELF_CLASS_64)
        return false;

    /* Fuck off then oy */
    if (header->e_type != ET_EXEC && header->e_type != ET_DYN)
        return false;

    return true;
}

kerror_t elf_grab_sheaders(file_t* file, struct elf64_hdr* header)
{
    int error;
    size_t read_size;

    if (!file || !header)
        return -1;

    read_size = sizeof(struct elf64_hdr);

    error = elf_read(file, header, &read_size, 0);

    if (error)
        return -1;

    if (!elf_verify_header(header))
        return -1;

    return (0);
}

/*!
 * @brief: Initializes the ELF execution methods
 */
void init_elf_exec_method()
{
    init_dynamic_elf_exec();
}
