#include "elf.h"
#include "dev/debug/serial.h"
#include "fs/file.h"
#include <fs/vobj.h>
#include "libk/bin/elf_types.h"
#include "libk/flow/error.h"
#include "libk/stddef.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/pg.h"
#include "mem/zalloc.h"
#include "proc/core.h"
#include "proc/proc.h"
#include "sched/scheduler.h"

static int elf_read(file_t* file, void* buffer, size_t* size, uintptr_t offset) 
{
  return file_read(file, buffer, size, offset);
}

/*!
 * @brief Copy the contents of an ELF into the correct user pages
 *
 * When mapping a userrange scattered, we need to first read the file into a contiguous kernel
 * region, after which we can loop over all the userpages and find their physical addresses. 
 * We translate these to high kernel addresses so that we can copy the correct bytes over
 */
//static int elf_read_into_user_range(pml_entry_t* root, file_t* file, vaddr_t user_start, size_t size, uintptr_t offset);

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

    if ((shdr->sh_flags & SHF_ALLOC) && strcmp(name, str_start + shdr->sh_name) == 0)
      return i;
  }

  return 0;
}

ErrorOrPtr elf_grab_sheaders(file_t* file, struct elf64_hdr* header)
{
  int error;
  size_t read_size;

  if (!file || !header)
    return Error();

  read_size = sizeof(struct elf64_hdr);

  error = elf_read(file, header, &read_size, 0);

  if (error < 0)
    return Error();

  /* No elf? */
  if (!memcmp(header->e_ident, ELF_MAGIC, ELF_MAGIC_LEN))
    return Error();

  /* No 64 bit binary =( */
  if (header->e_ident[EI_CLASS] != ELF_CLASS_64)
    return Error();

  return Success(0);
}

/*!
 * @brief: Tries to prepare a .elf file for execution
 *
 * @returns: the proc_id wrapped in Success() on success. Otherwise Error()
 * 
 * FIXME: do we close the file if this function fails?
 * FIXME: flags?
 */
ErrorOrPtr elf_exec_static_64_ex(file_t* file, bool kernel, bool defer_schedule) {

  proc_id_t id;
  proc_t* proc = NULL;
  struct elf64_phdr* phdrs = NULL;
  struct elf64_hdr header;
  struct proc_image image;
  uintptr_t proc_flags;
  uint32_t page_flags;
   
  if (IsError(elf_grab_sheaders(file, &header)))
    return Error();

  phdrs = elf_load_phdrs_64(file, &header);

  if (!phdrs)
    return Error();

  /* TODO: */

  page_flags = KMEM_FLAG_WRITABLE;
  proc_flags = NULL;

  /* When executing elf files in 'kernel' mode, they internally run as a driver */
  if (kernel)
    proc_flags |= PROC_DRIVER;

  proc = create_proc(nullptr, &id, (char*)file->m_obj->m_path, (void*)header.e_entry, 0, proc_flags);

  if (!proc)
    goto error_and_out;

  image.m_total_exe_bytes = file->m_total_size;
  image.m_lowest_addr = (vaddr_t)-1;
  image.m_highest_addr = 0;

  for (uintptr_t i = 0; i < header.e_phnum; i++) {
    struct elf64_phdr phdr = phdrs[i];

    switch (phdr.p_type) {
      case PT_LOAD:
        {
          vaddr_t virtual_phdr_base = phdr.p_vaddr;
          size_t phdr_size = phdr.p_memsz;

          vaddr_t v_user_phdr_start = Must(__kmem_alloc_range(
                proc->m_root_pd.m_root,
                proc->m_resource_bundle,
                virtual_phdr_base,
                phdr_size,
                KMEM_CUSTOMFLAG_GET_MAKE | KMEM_CUSTOMFLAG_CREATE_USER | KMEM_CUSTOMFLAG_NO_REMAP,
                page_flags
                ));

          vaddr_t v_kernel_phdr_start = Must(kmem_get_kernel_address(v_user_phdr_start, proc->m_root_pd.m_root));

          /* Copy elf into the mapped area */
          /* NOTE: we are required to be in the kernel map for this */
          elf_read(file, (void*)v_kernel_phdr_start, &phdr.p_filesz, phdr.p_offset);
          // ???

          if (phdr.p_memsz > phdr.p_filesz)
            memset((void*)(v_kernel_phdr_start + phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);

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

  kfree(phdrs);

  /* Copy over the image object */
  proc->m_image = image;

  /* NOTE: we can reschedule here, since the scheduler will give us our original pagemap back automatically */
  if (!defer_schedule)
    sched_add_priority_proc(proc, true);

  return Success(id);

error_and_out:
  /* TODO: clean more stuff */
  
  if (phdrs)
    kfree(phdrs);

  if (proc)
    destroy_proc(proc);

  return Error();

}
