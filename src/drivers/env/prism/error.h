#ifndef __ANIVA_PRISM_ERROR__
#define __ANIVA_PRISM_ERROR__

#include <libk/stddef.h>

struct prism_token;

enum PRISM_ERRCODE {
    PRISM_ERR_NONE = 0,
    PRISM_ERR_SYNTAX,
    PRISM_ERR_INTERNAL,
};

typedef struct prism_error {
    enum PRISM_ERRCODE code;
    struct prism_token* faulty_token;
    const char* description;
} prism_error_t;

static inline int init_prism_error(prism_error_t* error, enum PRISM_ERRCODE code, struct prism_token* token, const char* desc)
{
    error->code = code;
    error->description = desc;
    error->faulty_token = token;
    return code;
}

static inline bool prism_is_error(prism_error_t* error)
{
    return (error->code != PRISM_ERR_NONE);
}

#endif // !__ANIVA_PRISM_ERROR__
