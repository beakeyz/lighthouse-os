#ifndef __ANIVA_LIBK_ELF_TYPES__
#define __ANIVA_LIBK_ELF_TYPES__
#include <libk/stddef.h>

/* 32-bit ELF base types. */
typedef uint32_t	Elf32_Addr;
typedef uint16_t	Elf32_Half;
typedef uint32_t	Elf32_Off;
typedef int32_t 	Elf32_Sword;
typedef uint32_t	Elf32_Word;

/* 64-bit ELF base types. */
typedef uint64_t	Elf64_Addr;
typedef uint16_t	Elf64_Half;
typedef int16_t     Elf64_SHalf;
typedef uint64_t	Elf64_Off;
typedef int32_t 	Elf64_Sword;
typedef uint32_t	Elf64_Word;
typedef uint64_t	Elf64_Xword;
typedef int64_t     Elf64_Sxword;

#define PT_NULL     0x00000000
#define PT_LOAD     0x00000001
#define PT_DYNAMIC  0x00000002
#define PT_INTERP   0x00000003
#define PT_NOTE     0x00000004
#define PT_SHLIB    0x00000005
#define PT_PHDR     0x00000006
#define PT_TLS      0x00000007

#define ET_NONE     0x00000000
#define ET_REL      0x00000001
#define ET_EXEC     0x00000002
#define ET_DYN      0x00000003
#define ET_CORE     0x00000004

#define DT_NULL     0x00000000
#define DT_NEEDED   0x00000001
#define DT_PLTRELSZ 0x00000002
#define DT_PLTGOT   0x00000003
#define DT_HASH     0x00000004
#define DT_STRTAB   0x00000005
#define DT_SYMTAB   0x00000006
#define DT_RELA     0x00000007
#define DT_RELASZ   0x00000008
#define DT_RELAENT  0x00000009
#define DT_STRSZ    0x0000000A
#define DT_SYMENT   0x0000000B
#define DT_INIT     0x0000000C
#define DT_FINI     0x0000000D
#define DT_SONAME   0x0000000E
#define DT_RPATH    0x0000000F
#define DT_SYMBOLIC 0x00000010
#define DT_REL      0x00000011
#define DT_RELSZ    0x00000012
#define DT_RELENT   0x00000013
#define DT_PLTREL   0x00000014
#define DT_DEBUG    0x00000015
#define DT_TEXTREL  0x00000016
#define DT_JMPREL   0x00000017
#define DT_ENCODING 0x00000020

#define ABI_SYSV     0x00
#define ARCH_X86_64  0x3e
#define ARCH_X86_32  0x03
#define ARCH_AARCH64 0xb7
#define BITS_LE      0x01
#define SHT_RELA     0x00000004
#define R_X86_64_RELATIVE  0x00000008
#define R_AARCH64_RELATIVE 0x00000403

#define ELF_MAGIC_0     0x7F
#define ELF_MAGIC_1     'E'
#define ELF_MAGIC_2     'L'
#define ELF_MAGIC_3     'F'
#define ELF_MAGIC       "\177ELF"
#define ELF_MAGIC_LEN   4

#define ELF_CLASS_NONE  0
#define ELF_CLASS_32    1
#define ELF_CLASS_64    2
#define ELF_CLASS_NUM   3

/* Indices into identification array */
#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_OSABI    7
#define EI_PADDING  8

#define BITS64 64
#define BITS32 32

#define PF_R		0x4
#define PF_W		0x2
#define PF_X		0x1

#define EI_NIDENT	16

/* TODO: relative, dynamic and sym */

typedef struct elf32_hdr {
  unsigned char	e_ident[EI_NIDENT];
  Elf32_Half	e_type;
  Elf32_Half	e_machine;
  Elf32_Word	e_version;
  Elf32_Addr	e_entry;  /* Entry point */
  Elf32_Off	e_phoff;
  Elf32_Off	e_shoff;
  Elf32_Word	e_flags;
  Elf32_Half	e_ehsize;
  Elf32_Half	e_phentsize;
  Elf32_Half	e_phnum;
  Elf32_Half	e_shentsize;
  Elf32_Half	e_shnum;
  Elf32_Half	e_shstrndx;
} Elf32_Ehdr;

typedef struct elf64_hdr {
  unsigned char	e_ident[EI_NIDENT];	/* ELF "magic number" */
  Elf64_Half e_type;
  Elf64_Half e_machine;
  Elf64_Word e_version;
  Elf64_Addr e_entry;		/* Entry point virtual address */
  Elf64_Off e_phoff;		/* Program header table file offset */
  Elf64_Off e_shoff;		/* Section header table file offset */
  Elf64_Word e_flags;
  Elf64_Half e_ehsize;
  Elf64_Half e_phentsize;
  Elf64_Half e_phnum;
  Elf64_Half e_shentsize;
  Elf64_Half e_shnum;
  Elf64_Half e_shstrndx;
} Elf64_Ehdr;

typedef struct elf32_phdr {
  Elf32_Word	p_type;
  Elf32_Off	p_offset;
  Elf32_Addr	p_vaddr;
  Elf32_Addr	p_paddr;
  Elf32_Word	p_filesz;
  Elf32_Word	p_memsz;
  Elf32_Word	p_flags;
  Elf32_Word	p_align;
} Elf32_Phdr;

typedef struct elf64_phdr {
  Elf64_Word p_type;
  Elf64_Word p_flags;
  Elf64_Off p_offset;		/* Segment file offset */
  Elf64_Addr p_vaddr;		/* Segment virtual address */
  Elf64_Addr p_paddr;		/* Segment physical address */
  Elf64_Xword p_filesz;		/* Segment size in file */
  Elf64_Xword p_memsz;		/* Segment size in memory */
  Elf64_Xword p_align;		/* Segment alignment, file & memory */
} Elf64_Phdr;

#endif // !__ANIVA_LIBK_ELF_TYPES__
