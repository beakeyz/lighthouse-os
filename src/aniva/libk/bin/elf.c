#include "elf.h"
#include "dev/debug/serial.h"
#include "fs/file.h"
#include <fs/vobj.h>
#include "libk/bin/elf_types.h"
#include "libk/error.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/pg.h"
#include "mem/zalloc.h"
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

  if (!elf || !elf_header)
    return nullptr;
  
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
  struct elf_image image;
  uintptr_t proc_flags;
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

  proc_flags = PROC_DEFERED_HEAP;

  if (kernel)
    proc_flags |= PROC_KERNEL;

          println("Allocating funnie");
  ret = create_proc((char*)file->m_obj->m_path, (void*)header.e_entry, 0, proc_flags);
          println("Allocating funnie");

  image.m_total_exe_bytes = file->m_size;
  image.m_lowest_addr = (vaddr_t)-1;
  image.m_highest_addr = 0;

  for (uintptr_t i = 0; i < header.e_phnum; i++) {
    struct elf64_phdr phdr = phdrs[i];

    switch (phdr.p_type) {
      case PT_LOAD:
        {
          vaddr_t virtual_phdr_base = phdr.p_vaddr;
          size_t phdr_size = phdr.p_memsz;

          println("Allocating funnie");

          vaddr_t alloc_result = (vaddr_t)Must(kmem_map_and_alloc_range(
                ret->m_root_pd.m_root,
                phdr_size,
                virtual_phdr_base,
                KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER,
                KMEM_FLAG_WRITABLE));

          println("Allocated funnie");
          println(to_string(alloc_result));
          println(to_string(kmem_get_page_addr(kmem_get_page(ret->m_root_pd.m_root, alloc_result, 0)->raw_bits)));

          /* Copy elf into the mapped area */
          /* NOTE: we are required to be in the kernel map for this */
          elf_read(file, (void*)kmem_ensure_high_mapping(alloc_result), phdr.p_filesz, phdr.p_offset);
          memset((void*)(virtual_phdr_base + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);

          if ((alloc_result + phdr_size) > image.m_highest_addr) {
            image.m_highest_addr = alloc_result + phdr_size;
          }
          if (alloc_result < image.m_lowest_addr) {
            image.m_lowest_addr = alloc_result;
          }
        }
        break;
    }
  }

  println("Mapped PT_LOAD phdrs of elf");
  // TODO: make heap compatible with this shit
  // ret->m_heap = create_zone_allocator_ex(ret->m_root_pd, ALIGN_UP(image.m_highest_addr, SMALL_PAGE_SIZE), 10 * Kib, 10 * Mib, 0)->m_heap;

  // ASSERT_MSG(ret->m_root_pd.m_root, "No root");

  kfree(phdrs);
  return ret;

error_and_out:
  /* TODO: clean more stuff */
  kfree(phdrs);
  destroy_proc(ret);

  return nullptr;

}
