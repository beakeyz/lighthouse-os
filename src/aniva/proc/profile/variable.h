#ifndef __ANIVA_VARIABLE__
#define __ANIVA_VARIABLE__

#include "LibSys/proc/var_types.h"
#include "sync/atomic_ptr.h"
#include <libk/stddef.h>
#include <LibSys/proc/var_types.h>

struct proc_profile;

/*
 * Profile variables
 *
 * What are they used for:
 * We want to use profile variables as a way to dynamically store relevant 
 * information about the environment to the kernel or to processes. They might 
 * hold paths to important binaries for certain profiles or perhaps even the
 * version of the system. Any process can read the open data on any profile,
 * but only processes of the profile or a higher profile may modify the data.
 *
 * Variable key conventions:
 * The key for a variable can be anything pretty much, but we would like to keep the 
 * character count per key to a minimum. However, it is common for most system-assigned
 * variables to have keys in all-caps. Processes can make variables like this as well, of 
 * course, but we will never make assumptions about a variable, based on the capitalisation
 * of it's key
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
