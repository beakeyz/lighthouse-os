#include "elf.h"
#include <dev/kterm/kterm.h>
#include "dev/debug/serial.h"
#include "fs/file.h"
#include <fs/vobj.h>
#include "interupts/interupts.h"
#include "libk/bin/elf_types.h"
#include "libk/error.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/pg.h"
#include "mem/zalloc.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

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
ErrorOrPtr elf_exec_static_64(file_t* file, bool kernel) {

  proc_t* ret;
  struct elf64_hdr header;
  struct elf64_phdr* phdrs;
  struct elf_image image;
  uintptr_t proc_flags;
  uint32_t page_flags;
  int status;

  disable_interrupts();

  println_kterm("Reaing elf");
  status = elf_read(file, &header, sizeof(struct elf64_hdr), 0);

  /* No filE??? */
  if (status)
    return Error();

  /* No elf? */
  if (!memcmp(header.e_ident, ELF_MAGIC, ELF_MAGIC_LEN))
    return Error();

  /* No 64 bit binary =( */
  if (header.e_ident[EI_CLASS] != ELF_CLASS_64)
    return Error();

  println_kterm("Loading phdrs");
  phdrs = elf_load_phdrs_64(file, &header);

  if (!phdrs)
    return Error();

  /* TODO: */

  page_flags = KMEM_FLAG_WRITABLE;
  proc_flags = PROC_DEFERED_HEAP;

  if (kernel) {
    /* When executing elf files in 'kernel' mode, they internally run as a driver */
    proc_flags |= PROC_DRIVER;
  }

  ret = create_proc((char*)file->m_obj->m_path, (void*)header.e_entry, 0, proc_flags);

  image.m_total_exe_bytes = file->m_size;
  image.m_lowest_addr = (vaddr_t)-1;
  image.m_highest_addr = 0;

  println("Ding");
  for (uintptr_t i = 0; i < header.e_phnum; i++) {
    struct elf64_phdr phdr = phdrs[i];

    switch (phdr.p_type) {
      case PT_LOAD:
        {
          vaddr_t virtual_phdr_base = phdr.p_vaddr;
          size_t phdr_size = phdr.p_memsz;

          println("Alloc range");
          vaddr_t kalloc = Must(__kmem_kernel_alloc_range(phdr_size, KMEM_CUSTOMFLAG_GET_MAKE, 0));

          println("Map range");
          println(to_string(kalloc));
          println(to_string(virtual_phdr_base));
          println(to_string(phdr_size));
          println(to_string((uintptr_t)kmem_to_phys(nullptr, (uintptr_t)ret->m_root_pd.m_root)));
          Must(kmem_map_into(ret->m_root_pd.m_root, kalloc, virtual_phdr_base, phdr_size, KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER, KMEM_FLAG_WRITABLE));

          println("Read range");
          /* Copy elf into the mapped area */
          /* NOTE: we are required to be in the kernel map for this */
          elf_read(file, (void*)kalloc, phdr.p_filesz, phdr.p_offset);
          // ???
          // memset((void*)(virtual_phdr_base + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);

          println("Set range");
          if ((virtual_phdr_base + phdr_size) > image.m_highest_addr) {
            image.m_highest_addr = virtual_phdr_base + phdr_size;
          }
          if (virtual_phdr_base < image.m_lowest_addr) {
            image.m_lowest_addr = virtual_phdr_base;
          }
        }
        break;
    }
  }

  // TODO: make heap compatible with this shit
  // ret->m_heap = create_zone_allocator_ex(ret->m_root_pd, ALIGN_UP(image.m_highest_addr, SMALL_PAGE_SIZE), 10 * Kib, 10 * Mib, 0)->m_heap;

  // ASSERT_MSG(ret->m_root_pd.m_root, "No root");

  kfree(phdrs);

  println("Schedule");
  /* NOTE: we can reschedule here, since the scheduler will give us our original pagemap back automatically */
  sched_add_priority_proc(ret, true);

  return Success(0);

error_and_out:
  /* TODO: clean more stuff */
  kfree(phdrs);
  destroy_proc(ret);

  return Error();

}
