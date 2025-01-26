#ifndef __LIGHTOS_TOMLPRSR_H__
#define __LIGHTOS_TOMLPRSR_H__

#include <lightos/types.h>

/*
 * Token representation for a single token we encounter during the parsing
 * procedure of the toml file
 */
typedef struct toml_token {
    const char* tok;
    enum TOML_TOKEN_TYPE {
        TOML_TOKEN_TYPE_GROUP_START, // '['
        TOML_TOKEN_TYPE_GROUP_END, // ']'
        TOML_TOKEN_TYPE_LIST_START, // '['
        TOML_TOKEN_TYPE_LIST_END, // ']'
        TOML_TOKEN_TYPE_IDENTIFIER, // Anything that gives some value a name
        TOML_TOKEN_TYPE_ASSIGN, // '='
        TOML_TOKEN_TYPE_STRING, // Anyting in between "" or ''
        TOML_TOKEN_TYPE_NUM, // Just any number lmao
        TOML_TOKEN_TYPE_OBJ_START, // '{'
        TOML_TOKEN_TYPE_OBJ_END, // '}'
        TOML_TOKEN_TYPE_COMMA, // ','
        TOML_TOKEN_TYPE_NEWLINE, // '\n', EOS (end of statement)

        TOML_TOKEN_TYPE_UNKNOWN,
    } type;
    struct toml_token* next;
} toml_token_t;

static inline bool toml_token_istype(toml_token_t* token, enum TOML_TOKEN_TYPE type)
{
    return (token && token->type == type);
}

enum TOML_VALUE_TYPE {
    TOML_VALUE_TYPE_STRING,
    TOML_VALUE_TYPE_NUM,
    TOML_VALUE_TYPE_LIST,
    TOML_VALUE_TYPE_GROUP,
};

/*
 * After parsing a TOML file, we recieve a list of toml values. These guys
 * hold the actual key-value pairs toml has
 */
typedef struct toml_value {
    const char* key;
    enum TOML_VALUE_TYPE type;
    union {
        const char* string_value;
        u64 num_value;
        struct toml_value* list_value;
        struct toml_value* group_value;

        void* value;
    };
    struct toml_value* next;
} toml_value_t;

#endif // !__LIGHTOS_TOMLPRSR_H__
