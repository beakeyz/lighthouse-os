#ifndef __LIGHTENV_SYS_VAR_TYPES__
#define __LIGHTENV_SYS_VAR_TYPES__

#include <stdint.h>

/*
 * The different types that a profile variable can hold
 *
 * FIXME: we should make this naming sceme consistent with PVAR_FLAG___ naming
 */
enum PROFILE_VAR_TYPE {
  PROFILE_VAR_TYPE_STRING = 0,
  PROFILE_VAR_TYPE_BYTE,
  PROFILE_VAR_TYPE_WORD,
  PROFILE_VAR_TYPE_DWORD,
  PROFILE_VAR_TYPE_QWORD,
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
#define PVR_SIG_3 '\5'

#define PVR_SIG "PvR\5"

typedef struct pvr_file_header {
  const char sign[4];
  uint32_t strtab_offset;
  uint32_t var_count;
  uint32_t var_start_offset;
} pvr_file_header_t;

/*
 * If the type of a variable is a string, value_off will still just be a offset into the
 * value table, but that value will then be an offset into the string table, since the value
 * table only holds qword values
 */
typedef struct pvr_file_var {
  uint32_t str_off;
  uint32_t value_off;
  uint32_t var_flags;
  enum PROFILE_VAR_TYPE var_type;
} __attribute__((packed)) pvr_file_var_t;

typedef const char pvr_file_strtab_entry[];
typedef uint64_t   pvr_file_valtab_entry;

#endif // !__LIGHTENV_SYS_VAR_TYPES__
