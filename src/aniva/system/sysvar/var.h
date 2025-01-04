#ifndef __ANIVA_VARIABLE__
#define __ANIVA_VARIABLE__

#include <libk/stddef.h>
#include <sync/atomic_ptr.h>
#include <system/profile/attr.h>
#include <mem/tracker/tracker.h>
#include <lightos/sysvar/shared.h>

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
        /* Most basic values of a sysvar. These can be read almost directly, without having to do weird buffer shit */
        uint64_t qword_value;
        uint32_t dword_value;
        uint16_t word_value;
        uint8_t byte_value;
        /*
         * NOTE: when passing a string to a profile variable, the ownership of the string memory
         * is passed to this variable object. This means that on a value-change or variable destruction,
         * the memory gets freed if it was allocated on the heap.
         */
        const char* str_value;
        /* Range object owned by us */
        page_range_t* range_value;
        void* value;
    };
    enum SYSVAR_TYPE type;
    uint32_t flags;
    uint32_t len;
    uint32_t refc;
} sysvar_t;

void init_sysvars(void);

sysvar_t* create_sysvar(const char* key, enum PROFILE_TYPE ptype, enum SYSVAR_TYPE type, uint8_t flags, void* buffer, size_t bsize);

sysvar_t* get_sysvar(sysvar_t* var);
void release_sysvar(sysvar_t* var);
void sysvar_lock(sysvar_t* var);
void sysvar_unlock(sysvar_t* var);

/* Integer read functions */
u64 sysvar_read_u64(sysvar_t* var);
u32 sysvar_read_u32(sysvar_t* var);
u16 sysvar_read_u16(sysvar_t* var);
u8 sysvar_read_u8(sysvar_t* var);

const char* sysvar_read_str(sysvar_t* var);

/* These routines enable anyone to read/write anything to a sysvar, as long as it's inside the bounds of its size */
error_t sysvar_write(sysvar_t* var, void* buffer, size_t length);
error_t sysvar_read(sysvar_t* var, void* buffer, size_t length);
error_t sysvar_sizeof(sysvar_t* var, size_t* psize);

#endif // !__ANIVA_VARIABLE__
