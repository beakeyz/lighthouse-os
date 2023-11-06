#ifndef __ANIVA_VARIABLE__
#define __ANIVA_VARIABLE__

#include "LibSys/proc/var_types.h"
#include "sync/atomic_ptr.h"
#include <libk/stddef.h>

struct proc_profile;

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
 * Profile variables
 *
 * What are they used for:
 * We want to use profile variables as a way to dynamically store relevant 
 * information about the environment to the kernel or to processes. They might 
 * hold paths to important binaries for certain profiles or perhaps even the
 * version of the system. Any process can read the open data on any profile,
 * but only processes of the profile or a higher profile may modify the data.
 */
typedef struct profile_var {
  struct proc_profile* profile;
  const char* key;
  union {
    const char* str_value;
    uint64_t qword_value;
    uint32_t dword_value;
    uint16_t word_value;
    uint8_t byte_value;
    void* value;
  };
  enum PROFILE_VAR_TYPE type;
  uint32_t flags;
  uint32_t len;
  atomic_ptr_t* refc;
} profile_var_t;

void init_proc_variables(void);

profile_var_t* create_profile_var(const char* key, enum PROFILE_VAR_TYPE type, uint8_t flags, uintptr_t value);

profile_var_t* get_profile_var(profile_var_t* var);
void release_profile_var(profile_var_t* var);

bool profile_var_get_str_value(profile_var_t* var, const char** buffer);
bool profile_var_get_qword_value(profile_var_t* var, uint64_t* buffer);
bool profile_var_get_dword_value(profile_var_t* var, uint32_t* buffer);
bool profile_var_get_word_value(profile_var_t* var, uint16_t* buffer);
bool profile_var_get_byte_value(profile_var_t* var, uint8_t* buffer);

bool profile_var_write(profile_var_t* var, uint64_t value);

#endif // !__ANIVA_VARIABLE__
