#ifndef __LIGHTENV_SYS_VAR_TYPES__
#define __LIGHTENV_SYS_VAR_TYPES__

#include <stdint.h>

/*
 * The different types that a profile variable can hold
 *
 * FIXME: we should make this naming sceme consistent with PVAR_FLAG___ naming
 */
enum SYSVAR_TYPE {
  SYSVAR_TYPE_STRING = 0,
  SYSVAR_TYPE_BYTE,
  SYSVAR_TYPE_WORD,
  SYSVAR_TYPE_DWORD,
  SYSVAR_TYPE_QWORD,

  SYSVAR_TYPE_UNSET = 0xffffffff
};

/* Open for anyone to read */
#define PVAR_FLAG_GLOBAL (0x00000001)
/* Open for anyone to modify? */
#define PVAR_FLAG_CONSTANT (0x00000002)
/* Holds data that the system probably needs to function correctly */
#define PVAR_FLAG_VOLATILE (0x00000004)
/* Holds configuration data */
#define PVAR_FLAG_CONFIG (0x00000008)
/* Hidden to any profiles with lesser permissions */
#define PVAR_FLAG_HIDDEN (0x00000010)

/*
 * Structures for the .pvr files
 */

#define PVR_SIG_0 'P'
#define PVR_SIG_1 'v'
#define PVR_SIG_2 'R'
/* (PVR_SIG_0 + PVR_SIG_1 + PVR_SIG_2 - 1) / 3 */
#define PVR_SIG_3 ']'

#define PVR_SIG "PvR]"

typedef struct pvr_file_header {
  const char sign[4];
  uint32_t var_count;
  uint32_t var_capacity;
  uint32_t kernel_version;
  uint32_t strtab_offset;
  uint32_t valtab_offset;
} pvr_file_header_t;

/*
 * If the type of a variable is a string, value_off will still just be a offset into the
 * value table, but that value will then be an offset into the string table, since the value
 * table only holds qword values
 */
typedef struct pvr_file_var {
  uint32_t key_off;
  uint32_t value_off;
  uint32_t key_len;
  uint32_t val_len;
  uint32_t var_flags;
  enum SYSVAR_TYPE var_type;
} pvr_file_var_t;

typedef char pvr_file_strtab_entry[];

#define VALTAB_ENTRY_FLAG_USED 0x01

#define VALTAB_ENTRY_ISUSED(entry) (((entry)->flags & VALTAB_ENTRY_FLAG_USED) == VALTAB_ENTRY_FLAG_USED)

typedef struct pvr_file_valtab_entry {
  uint8_t flags;
  uint8_t _res[7];
  uintptr_t value;
} pvr_file_valtab_entry_t;

typedef struct pvr_file_strtab {
  uint32_t next_valtab_off;
  uint32_t bytecount;
  /* All entries are combined into a single string and seperated by null-bytes */
  pvr_file_strtab_entry entries;
} pvr_file_strtab_t;

typedef struct pvr_file_valtab {
  uint32_t next_valtab_off;
  uint32_t capacity;
  pvr_file_valtab_entry_t entries[];
} pvr_file_valtab_t;

#endif // !__LIGHTENV_SYS_VAR_TYPES__
