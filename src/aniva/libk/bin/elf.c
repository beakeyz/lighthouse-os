#include "elf.h"
#include "fs/file.h"
#include "libk/bin/elf_types.h"
#include "libk/stddef.h"
#include "mem/heap.h"
#include "proc/proc.h"

/*
 * Represents the elf file when it is eventually laid out in memory.
 */
struct elf_image {
  vaddr_t m_lowest_addr;
  vaddr_t m_highest_addr;
  size_t m_total_exe_bytes;
};

static int elf_read(file_t* file, void* buffer, size_t size, uintptr_t offset) {
  return file->m_ops->f_read(file, buffer, size, offset);
}

static struct elf64_phdr* elf_load_phdrs_64(file_t* elf, struct elf64_hdr* elf_header) {

  struct elf64_phdr* ret;
  int read_res;
  size_t total_size;

  
  total_size = elf_header->e_phnum * sizeof(struct elf64_phdr);

  if (!total_size || total_size > 0x10000)
    return nullptr;

  ret = kmalloc(total_size);

  if (!ret)
    return nullptr;

  read_res = elf->m_ops->f_read(elf, ret, total_size, elf_header->e_phoff);

  if (read_res) {
    kfree(ret);
    return nullptr;
  }

  return ret;
}

/*
 * FIXME: do we clos the file if this function fails?
 */
proc_t* elf_exec_static_64(file_t* file, bool kernel) {

  proc_t* ret;
  struct elf64_hdr header;
  struct elf64_phdr* phdrs;
  int status;

  status = elf_read(file, &header, sizeof(struct elf64_hdr), 0);

  /* No filE??? */
  if (status)
    return nullptr;

  /* No elf? */
  if (!memcmp(header.e_ident, ELF_MAGIC, ELF_MAGIC_LEN))
    return nullptr;

  /* No 64 bit binary =( */
  if (header.e_ident[EI_CLASS] != ELF_CLASS_64)
    return nullptr;

  phdrs = elf_load_phdrs_64(file, &header);

  if (!phdrs)
    return nullptr;

  /* TODO: */

  return ret;
}
