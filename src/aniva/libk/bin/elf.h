#ifndef __ANIVA_LIBK_ELF_BIN__
#define __ANIVA_LIBK_ELF_BIN__

#include "fs/file.h"
#include "libk/bin/elf_types.h"
#include "libk/flow/error.h"
#include <libk/stddef.h>

/*
 * Create a process that starts execution at the entrypoint of the elf file
 * and imidiately yield to it
 */
ErrorOrPtr elf_exec_64(file_t* file, bool kernel, bool defer_schedule);
ErrorOrPtr elf_exec_32(file_t* file, bool kernel, bool defer_schedule);

int elf_read(file_t* file, void* buffer, size_t* size, uintptr_t offset);
bool elf_verify_header(struct elf64_hdr* header);

ErrorOrPtr elf_grab_sheaders(file_t* file, struct elf64_hdr* header);
struct elf64_phdr* elf_load_phdrs_64(file_t* elf, struct elf64_hdr* elf_header);

uint32_t elf_find_section(struct elf64_hdr* header, const char* name);
const char* elf_get_shdr_name(struct elf64_hdr* header, struct elf64_shdr* shdr);
void* elf_section_start_size(struct elf64_hdr* header, const char* name);
void* elf_section_start_addr(struct elf64_hdr* header, const char* name);
struct elf64_shdr* elf_get_shdr(struct elf64_hdr* header, uint32_t i);

void init_dynamic_loader();

#endif // !__ANIVA_LIBK_ELF_BIN__
