#ifndef __ANIVA_PRISM_BYTECODE__
#define __ANIVA_PRISM_BYTECODE__

#include <libk/stddef.h>

#define PRISM_BYTECODE_SIG 0x1f

/*
 * Single prism bytecode instruction
 */
typedef struct raw_bytecode_instruction {
    uint8_t signature;
    uint8_t instruction;
    uint8_t params[];
} raw_bytecode_instruction_t;

enum BYTECODE_INSTRUCTIONS {
    PRISM_BYTECODE_LABEL,
    PRISM_BYTECODE_VAR,
    PRISM_BYTECODE_ADD,
    PRISM_BYTECODE_SUB,
    PRISM_BYTECODE_CALL,
    PRISM_BYTECODE_EXIT,
    PRISM_BYTECODE_TO,
    PRISM_BYTECODE_CPY,
    PRISM_BYTECODE_XOR,
    PRISM_BYTECODE_OR,
    PRISM_BYTECODE_AND,
    PRISM_BYTECODE_FUN,
    PRISM_BYTECODE_MOD,
    PRISM_BYTECODE_JIF,
    PRISM_BYTECODE_JMP,
};

#define PRISM_BYTE_TOK_DEF_RETVAR "->"
#define PRISM_BYTE_TOK_RET "<-"

/*
 * State machine for prism bytecode
 */
typedef struct prism_byte_executor {
    void* code;
    size_t code_len;
    raw_bytecode_instruction_t* c_instr;

    size_t stack_size;
    void* stack;
    void* stack_ptr;
    void* stack_base;

    size_t varstack_size;
    void* varstack;
} prism_byte_executor_t;

typedef struct prism_byte_op {
    enum BYTECODE_INSTRUCTIONS id;
    uint32_t param_count;

    uintptr_t (*f_exec)(prism_byte_executor_t* executor, struct prism_byte_op* op);

    enum {
        PARAMTYPE_INT64 = 8,
        PARAMTYPE_INT32 = 4,
        PARAMTYPE_INT16 = 2,
        PARAMTYPE_INT8 = 1,
        PARAMTYPE_STRING = 0,
    } params[8];
} prism_byte_op_t;

#endif // !__ANIVA_PRISM_BYTECODE__
