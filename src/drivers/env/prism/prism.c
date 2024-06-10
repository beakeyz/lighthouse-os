#include "prism.h"
#include "libk/stddef.h"
#include <string.h>

struct prism_keyword {
    const char* label;
    enum PRISM_KEYWORD id;
} __prism_keywords[] = {
    {
        "mode",
        PRISM_KEYWORD_MODE,
    },
    {
        "func",
        PRISM_KEYWORD_FUNC,
    },
};

struct prism_operator {
    const char* label;
    enum PRISM_OPERATOR id;
} __prism_operators[] = {
    {
        "=",
        PRISM_OPERATOR_ASSIGNMENT,
    },
    {
        ":",
        PRISM_OPERATOR_TYPE,
    },
    {
        "+",
        PRISM_OPERATOR_ADD,
    },
    {
        "-",
        PRISM_OPERATOR_SUBTRACT,
    },
};

struct _prism_encapsulator {
    const char* lables;
    enum PRISM_ENCAPSULATOR id;
} __prism_encapsulators[] = {
    {
        "{}",
        PRISM_ENCAPSULATOR_SCOPE,
    },
    {
        "()",
        PRISM_ENCAPSULATOR_CAST,
    },
    {
        "[]",
        PRISM_ENCAPSULATOR_FUNC_PARAMS,
    },
};

static uint32_t __keyword_count = arrlen(__prism_keywords);
static uint32_t __operator_count = arrlen(__prism_operators);
static uint32_t __encapsulator_count = arrlen(__prism_encapsulators);

bool prism_is_valid_keyword(const char* label, uint32_t label_len)
{
    for (uint32_t i = 0; i < __keyword_count; i++) {
        if (strncmp(label, __prism_keywords[i].label, label_len) == 0)
            return true;
    }

    return false;
}

bool prism_is_valid_operator(const char* label, uint32_t label_len)
{
    for (uint32_t i = 0; i < __operator_count; i++) {
        if (strncmp(label, __prism_operators[i].label, label_len) == 0)
            return true;
    }

    return false;
}

bool prism_is_valid_encapsulator(char c, prism_encapsulator_t* b_out)
{
    for (uint32_t i = 0; i < __encapsulator_count; i++) {
        if (__prism_encapsulators[i].lables[0] != c && __prism_encapsulators[i].lables[1] != c)
            continue;

        if (b_out) {
            b_out->id = __prism_encapsulators[i].id;
            b_out->opening = (__prism_encapsulators[i].lables[0] == c);
        }
        return true;
    }

    return false;
}
