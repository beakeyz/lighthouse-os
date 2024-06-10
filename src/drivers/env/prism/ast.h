#ifndef __ANIVA_PRISM_AST__
#define __ANIVA_PRISM_AST__

#include "drivers/env/prism/error.h"
#include "drivers/env/prism/prism.h"
#include <libk/stddef.h>

enum PRISM_TOKEN_TYPE {
    PRISM_TOKEN_TYPE_INVALID,
    /* Reserved keywords */
    PRISM_TOKEN_TYPE_KEYWORD,
    /* Identifiers of functions, variables, ect. */
    PRISM_TOKEN_TYPE_IDENTIFIER,
    /* Things that force an action upon another object or token */
    PRISM_TOKEN_TYPE_OPERATOR,
    /* Spacers like parentheses or braces */
    PRISM_TOKEN_TYPE_ENCAPSULATOR,
};

/*
 * Tokens which are generated at runtime into an AST
 */
typedef struct prism_token {
    const char* label;
    enum PRISM_TOKEN_TYPE type;

    union {
        enum PRISM_KEYWORD keyword;
        enum PRISM_OPERATOR operator;
        prism_encapsulator_t encapsulator;
    } priv;

    struct prism_token* next;
} prism_token_t;

typedef struct prism_ast {
    uint32_t syntax_errors;
    uint32_t max_syntax_errors;
    prism_token_t* root;
} prism_ast_t;

prism_ast_t* create_prism_ast(const char* code, size_t code_len, prism_error_t* berror);
int destroy_prism_ast(prism_ast_t* ast);

int prism_ast_debug(prism_ast_t* ast);

#endif // !__ANIVA_PRISM_AST__
