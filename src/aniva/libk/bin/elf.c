#include "elf.h"
#include <dev/kterm/kterm.h>
#include "dev/debug/serial.h"
#include "fs/file.h"
#include <fs/vobj.h>
#include "interrupts/interrupts.h"
#include "libk/bin/elf_types.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/pg.h"
#include "mem/zalloc.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

static int elf_read(file_t* file, void* buffer, size_t* size, uintptr_t offset) {
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

  read_res = elf->m_ops->f_read(elf, ret, &total_size, elf_header->e_phoff);

  if (read_res) {
    kfree(ret);
    return nullptr;
  }

  return ret;
}

/*
 * FIXME: do we close the file if this function fails?
 * FIXME: flags?
 */
ErrorOrPtr elf_exec_static_64(file_t* file, bool kernel) {

  proc_t* ret;
  struct elf64_hdr header;
  struct elf64_phdr* phdrs;
  struct proc_image image;
  uintptr_t proc_flags;
  uint32_t page_flags;
  size_t read_size;
  int status;

  disable_interrupts();

  println_kterm("Reaing elf");
  read_size = sizeof(struct elf64_hdr);
  status = elf_read(file, &header, &read_size, 0);

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

  image.m_total_exe_bytes = file->m_buffer_size;
  image.m_lowest_addr = (vaddr_t)-1;
  image.m_highest_addr = 0;

  for (uintptr_t i = 0; i < header.e_phnum; i++) {
    struct elf64_phdr phdr = phdrs[i];

    switch (phdr.p_type) {
      case PT_LOAD:
        {
          vaddr_t virtual_phdr_base = phdr.p_vaddr;
          size_t phdr_size = phdr.p_memsz;

          println("Alloc range");

          vaddr_t v_user_phdr_start = Must(__kmem_alloc_range(
                ret->m_root_pd.m_root,
                ret,
                virtual_phdr_base,
                phdr_size,
                KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER | KMEM_CUSTOMFLAG_NO_REMAP,
                KMEM_FLAG_WRITABLE
                ));

          vaddr_t v_kernel_phdr_start = Must(kmem_get_kernel_address(v_user_phdr_start, ret->m_root_pd.m_root));

          println("Read range");
          /* Copy elf into the mapped area */
          /* NOTE: we are required to be in the kernel map for this */
          elf_read(file, (void*)v_kernel_phdr_start, &phdr.p_filesz, phdr.p_offset);
          // ???
          // memset((void*)(virtual_phdr_base + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);

          println("Set range");
          if ((virtual_phdr_base + phdr_size) > image.m_highest_addr) {
            image.m_highest_addr = ALIGN_UP(virtual_phdr_base + phdr_size, SMALL_PAGE_SIZE);
          }
          if (virtual_phdr_base < image.m_lowest_addr) {
            image.m_lowest_addr = ALIGN_DOWN(virtual_phdr_base, SMALL_PAGE_SIZE);
          }
        }
        break;
    }
  }

  // TODO: make heap compatible with this shit
  // ret->m_heap = create_zone_allocator_ex(ret->m_root_pd, ALIGN_UP(image.m_highest_addr, SMALL_PAGE_SIZE), 10 * Kib, 10 * Mib, 0)->m_heap;

  // ASSERT_MSG(ret->m_root_pd.m_root, "No root");

  kfree(phdrs);

  /* Copy over the image object */
  ret->m_image = image;

  /* NOTE: we can reschedule here, since the scheduler will give us our original pagemap back automatically */
  sched_add_priority_proc(ret, true);

  return Success(0);

error_and_out:
  /* TODO: clean more stuff */
  kfree(phdrs);
  destroy_proc(ret);

  return Error();

}
