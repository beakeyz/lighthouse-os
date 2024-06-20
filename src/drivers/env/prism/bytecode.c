#include "bytecode.h"

uint64_t _f_prism_add(prism_byte_executor_t* exec, prism_byte_op_t* op);

static prism_byte_op_t _ops[] = {
    {
        PRISM_BYTECODE_ADD,
        2,
        _f_prism_add,
        {
            PARAMTYPE_STRING,
            PARAMTYPE_INT64,
        },
    },
};

static char* code = {
    "label main:\n"
    /* Push a variable with a value onto the stack */
    "var test[str] \"This is a test\" \n"
    /* Push another variable */
    "var test_2[u64] 0x69ff69ff \n"
    /* Perform actions on this actor and store them there */
    "test sub test_2 \n"
    /* Another test */
    "test_2 add test \n"
    /* Prepare a variable */
    "var test_3[u64] \n"
    /* Call test function */
    "call _test(test, test_2) to test_3\n"
    /* We're done */
    "exit\n"
    /* Declare test function */
    "fun _test(a[u64], b[u64]) -> ret[u64]:\n"
    /* Clear ret */
    "ret xor ret\n"
    /* Copy a into ret */
    "ret cpy a\n"
    /* Subtract b from ret */
    "ret sub b\n"
    /* Return from the function */
    "<- ret\n"

    "fun print(str[string]) -> err[i32]:\n"
    /* Declare a module (constant int handle) */
    "var kern[module]\n"
    /* store the aniva module into kern */
    "kern mod aniva\n"
    /* declare a variable where we store the function */
    "var _print[fun(str[string])]\n"
    /* find function into _print */
    "_print, err ffn kern->klogln\n"
    /* Jump if */
    "jif (err != 0) print.end\n"
    /* Call the function */
    "call _print(str) to err\n"
    /* Local label inside a function */
    "label print.end:\n"
    "<- err\n"
};

static uint8_t bytecode[] = {
    PRISM_BYTECODE_VAR, 'T', 'e', 's', 't', '\0', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    PRISM_BYTECODE_SUB, 'T', 'e', 's', 't', '\0', 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
