#ifndef __LIGHTENV_SYS_VAR_TYPES__
#define __LIGHTENV_SYS_VAR_TYPES__

#include <stdint.h>

/*
 * The different types that a profile variable can hold
 *
 * FIXME: we should make this naming sceme consistent with PVAR_FLAG___ naming
 */
enum SYSVAR_TYPE {
    SYSVAR_TYPE_UNSET = 0,

    SYSVAR_TYPE_STRING,
    SYSVAR_TYPE_BYTE,
    SYSVAR_TYPE_WORD,
    SYSVAR_TYPE_DWORD,
    SYSVAR_TYPE_QWORD,
};

/* Open for anyone to read */
#define SYSVAR_FLAG_GLOBAL (0x00000001)
/* Open for anyone to modify? */
#define SYSVAR_FLAG_CONSTANT (0x00000002)
/* Holds data that the system probably needs to function correctly */
#define SYSVAR_FLAG_VOLATILE (0x00000004)
/* Holds configuration data */
#define SYSVAR_FLAG_CONFIG (0x00000008)
/* Hidden to any profiles with lesser permissions */
#define SYSVAR_FLAG_HIDDEN (0x00000010)
/* Can't be removed or changed. Only the value can change */
#define SYSVAR_FLAG_STATIC (0x00000020)

/*
 * Structures for the .pvr files
 */

#define SYSVAR_SIG_0 'P'
#define SYSVAR_SIG_1 'v'
#define SYSVAR_SIG_2 'R'
/* (PVR_SIG_0 + PVR_SIG_1 + PVR_SIG_2 - 1) / 3 */
#define SYSVAR_SIG_3 ']'

#define SYSVAR_SIG "PvR]"

typedef struct pvr_file_header {
    const char sign[4];
    uint32_t kernel_version;
    uint32_t strtab_offset;
    uint32_t varbuf_offset;
} pvr_file_header_t;

/*
 * If the type of a variable is a string, value_off will still just be a offset into the
 * value table, but that value will then be an offset into the string table, since the value
 * table only holds qword values
 */
typedef struct pvr_file_var {
    uint32_t key_off;
    uint32_t var_flags;
    enum SYSVAR_TYPE var_type;
    uint64_t var_value;
} pvr_file_var_t;

#define PVR_VAR_ISUSED(pvr_var) ((pvr_var)->key_off == 0 && (pvr_var)->var_type != SYSVAR_TYPE_UNSET)

typedef struct pvr_file_var_buffer {
    uint64_t next_valbuffer;
    uint32_t var_count;
    uint32_t var_capacity;
    pvr_file_var_t variables[];
} pvr_file_var_buffer_t;

/* TODO: Upgrade this shit */
typedef struct pvr_file_strtab_entry {
    uint16_t len;
    char str[];
} pvr_file_strtab_entry_t;

typedef struct pvr_file_strtab {
    uint32_t next_valtab_off;
    uint32_t bytecount;
    /* All entries are combined into a single string and seperated by null-bytes */
    pvr_file_strtab_entry_t entries[];
} pvr_file_strtab_t;

#endif // !__LIGHTENV_SYS_VAR_TYPES__
