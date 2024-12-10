#ifndef __LIGHTOS_VAR_SHARED__
#define __LIGHTOS_VAR_SHARED__

#include <stdint.h>

#define SYSVAR_PROCNAME "NAME"
#define SYSVAR_CMDLINE "CMDLINE"

/* Sysvar ids for standard i/o */
#define SYSVAR_STDIO_HANDLE_TYPE "IO-HANDLE"
#define SYSVAR_STDIO "IO-DUMP"
#define SYSVAR_IO_DUMP SYSVAR_STDIO

/*
 * The different types that a profile variable can hold
 *
 * FIXME: we should make this naming sceme consistent with PVAR_FLAG___ naming
 */
enum SYSVAR_TYPE {
    SYSVAR_TYPE_INVAL = -1,
    SYSVAR_TYPE_UNSET = 0,

    SYSVAR_TYPE_STRING,
    SYSVAR_TYPE_BYTE,
    SYSVAR_TYPE_WORD,
    SYSVAR_TYPE_DWORD,
    SYSVAR_TYPE_QWORD,
    SYSVAR_TYPE_GUID,
    SYSVAR_TYPE_PATH,
    SYSVAR_TYPE_ENUM,
};

/*
 * Template struct for building a sysvar
 *
 * This boi contains only the essensial stuff needed to create a sysvar.
 * The type of the sysvar is determined by the first byte inside the key.
 * This byte is skipped when generating the actual key for the sysvar
 */
struct sysvar_template {
    const char* key;
    union {
        uint64_t qwrd_val;
        uint32_t dwrd_val;
        uint16_t wrd_val;
        uint8_t byte_val;
        void* p_val;
        const char* str_val;
    };
};

static inline enum SYSVAR_TYPE sysvar_template_get_type(struct sysvar_template* tmp)
{
    switch (tmp->key[0]) {
    case 'q':
        return SYSVAR_TYPE_QWORD;
    case 'd':
        return SYSVAR_TYPE_DWORD;
    case 'w':
        return SYSVAR_TYPE_WORD;
    case 'b':
        return SYSVAR_TYPE_BYTE;
    case 's':
        return SYSVAR_TYPE_STRING;
    }

    return SYSVAR_TYPE_UNSET;
}

typedef struct sysvar_template sysvar_vector_t[];

#define SYSVAR_VEC_ENTRY(key, val)                    \
    (struct sysvar_template)                          \
    {                                                 \
        (const char*)(key), { .p_val = (void*)(val) } \
    }

#define SYSVAR_VEC_STR(key, val) SYSVAR_VEC_ENTRY("s" key, (uint64_t)val)
#define SYSVAR_VEC_BYTE(key, val) SYSVAR_VEC_ENTRY("b" key, (uint64_t)val)
#define SYSVAR_VEC_WORD(key, val) SYSVAR_VEC_ENTRY("w" key, (uint64_t)val)
#define SYSVAR_VEC_DWORD(key, val) SYSVAR_VEC_ENTRY("d" key, (uint64_t)val)
#define SYSVAR_VEC_QWORD(key, val) SYSVAR_VEC_ENTRY("q" key, (uint64_t)val)
#define SYSVAR_VEC_END SYSVAR_VEC_ENTRY(NULL, NULL)

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
 * table only holds u64 values
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

#endif // !__LIGHTOS_VAR_SHARED__
