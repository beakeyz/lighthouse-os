#ifndef __ANIVA_VARIABLE__
#define __ANIVA_VARIABLE__

#include "system/profile/attr.h"
#include <libk/stddef.h>
#include <lightos/sysvar/shared.h>
#include <sync/atomic_ptr.h>

struct user_profile;
struct oss_obj;

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
typedef struct sysvar {
    struct oss_obj* obj;
    const char* key;
    union {
        /*
         * NOTE: when passing a string to a profile variable, the ownership of the string memory
         * is passed to this variable object. This means that on a value-change or variable destruction,
         * the memory gets freed if it was allocated on the heap.
         */
        const char* str_value;
        uint64_t qword_value;
        uint32_t dword_value;
        uint16_t word_value;
        uint8_t byte_value;
        void* value;
    };
    enum SYSVAR_TYPE type;
    uint32_t flags;
    uint32_t len;
    atomic_ptr_t* refc;
} sysvar_t;

void init_sysvars(void);

sysvar_t* create_sysvar(const char* key, enum PROFILE_TYPE ptype, enum SYSVAR_TYPE type, uint8_t flags, uintptr_t value);

sysvar_t* get_sysvar(sysvar_t* var);
void release_sysvar(sysvar_t* var);

bool sysvar_get_str_value(sysvar_t* var, const char** buffer);
bool sysvar_get_qword_value(sysvar_t* var, uint64_t* buffer);
bool sysvar_get_dword_value(sysvar_t* var, uint32_t* buffer);
bool sysvar_get_word_value(sysvar_t* var, uint16_t* buffer);
bool sysvar_get_byte_value(sysvar_t* var, uint8_t* buffer);

bool sysvar_write(sysvar_t* var, uint64_t value);
int sysvar_read(sysvar_t* var, u8* buffer, size_t length);

#endif // !__ANIVA_VARIABLE__
