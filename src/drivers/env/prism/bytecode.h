#ifndef __ANIVA_PRISM_BYTECODE__
#define __ANIVA_PRISM_BYTECODE__

#include <libk/stddef.h>

/*
 * Single prism bytecode instruction
 */
typedef struct raw_bytecode_instruction {
  uint8_t instruction;
  uint8_t params[];
} raw_bytecode_instruction_t;

/*
 * Buffer to contain the actual code
 */
typedef struct prism_bytecode {
  size_t buffer_size;
  raw_bytecode_instruction_t code[];
} prism_bytecode_t;

enum BYTECODE_INSTRUCTIONS {
  PRISM_BYTECODE_ADD = 0x00,
  PRISM_BYTECODE_SUBTRACT,
  PRISM_BYTECODE_JUMP,
  PRISM_BYTECODE_VAR,
};

/*
 * State machine for prism bytecode
 */
typedef struct prism_byte_executor {
  prism_bytecode_t* code;
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

  uintptr_t (*f_exec) (prism_byte_executor_t* executor, struct prism_byte_op* op);

  enum {
    PARAMTYPE_INT64 = 8,
    PARAMTYPE_INT32 = 4,
    PARAMTYPE_INT16 = 2,
    PARAMTYPE_INT8 = 1,
    PARAMTYPE_STRING = 0,
  } params[8];
} prism_byte_op_t;

#endif // !__ANIVA_PRISM_BYTECODE__
