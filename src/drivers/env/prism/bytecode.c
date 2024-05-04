#include "bytecode.h"

uint64_t _f_prism_add(prism_byte_executor_t* exec, prism_byte_op_t* op);

static prism_byte_op_t _ops[] = {
  { PRISM_BYTECODE_ADD,         2,      _f_prism_add, { PARAMTYPE_STRING, PARAMTYPE_INT64, }, },
  { PRISM_BYTECODE_SUBTRACT,    2,      _f_prism_add, { PARAMTYPE_STRING, PARAMTYPE_INT64, }, },
  { PRISM_BYTECODE_JUMP,        1,      _f_prism_add, { PARAMTYPE_INT64, }, },
  { PRISM_BYTECODE_VAR,         2,      _f_prism_add, { PARAMTYPE_STRING, PARAMTYPE_INT64, }, },
};

static uint8_t code[] = {
  PRISM_BYTECODE_VAR, 'T', 'e', 's', 't', '\0', 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  PRISM_BYTECODE_SUBTRACT, 'T', 'e', 's', 't', '\0', 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
