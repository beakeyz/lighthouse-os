#ifndef __ANIVA_ACPI_OBJ__
#define __ANIVA_ACPI_OBJ__
#include <libk/stddef.h>
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/reference.h"
#include "structures.h"
#include "libk/string.h"

struct acpi_str;
struct acpi_buffer;
struct acpi_package;
struct acpi_operand;
struct acpi_invocation;
struct acpi_ns_node;
struct acpi_aml_seg;

// Value types: integer, string, buffer, package.
#define ACPI_INTEGER 1
#define ACPI_STRING 2
#define ACPI_BUFFER 3
#define ACPI_PACKAGE 4
// Handle type: this is used to represent device (and other) namespace nodes.
#define ACPI_HANDLE 5
#define ACPI_LAZY_HANDLE 6
// Reference types: obtained from RefOf() or CondRefOf()
#define ACPI_ARG_REF 7
#define ACPI_LOCAL_REF 8
#define ACPI_NODE_REF 9
// Index types: obtained from Index().
#define ACPI_STRING_INDEX 10
#define ACPI_BUFFER_INDEX 11
#define ACPI_PACKAGE_INDEX 12

enum acpi_obj_type {
  ACPI_TYPE_NONE,
  ACPI_TYPE_INTEGER,
  ACPI_TYPE_STRING,
  ACPI_TYPE_BUFFER,
  ACPI_TYPE_PACKAGE,
  ACPI_TYPE_DEVICE,
};

typedef struct {
  int var_type;
  uint64_t num;

  // data
  union {
    struct acpi_str* str_p;
    struct acpi_buffer* buffer_p;
    struct acpi_package* package_p;
    struct {
      struct acpi_invocation* ptr;
      int ref_index;
    } invocation_p;
  };

  struct acpi_ns_node* handle;

  int var_idx;
} acpi_variable_t;

typedef struct acpi_str {
  flat_refc_t rc;
  size_t size;
  char* str;
} acpi_str_t;

typedef struct acpi_buffer {
  flat_refc_t rc;

  size_t size;
  uint8_t* buffer;
} acpi_buffer_t;

typedef struct acpi_package {
  flat_refc_t rc;

  size_t pkg_size;
  acpi_variable_t* vars;
} acpi_package_t;

#define ACPI_OPERAND_OBJECT 1
#define ACPI_NULL_NAME 2
#define ACPI_UNRESOLVED_NAME 3
#define ACPI_RESOLVED_NAME 4
#define ACPI_ARG_NAME 5
#define ACPI_LOCAL_NAME 6
#define ACPI_DEBUG_NAME 7

typedef struct acpi_operand {
  flat_refc_t rc;
  int tag;

  union {
    acpi_variable_t obj;
    int idx;

    struct acpi_ns_node* handle;
  };

} acpi_operand_t;

/*
 * acpi invocation
 */
typedef struct acpi_invocation {
  // an invocation has a maximum of 7 arguments
  acpi_variable_t args[7];
  // with 8 locally
  acpi_variable_t local[8];

  list_t *resulting_ns_nodes;
} acpi_invocation_t;

typedef struct {
  struct acpi_aml_seg *segm;
  uint8_t* code;

  acpi_invocation_t* invocation;
  struct acpi_ns_node* ctx_handle;
} acpi_context_entry_t;

typedef struct {
  int program_counter;
  int program_counter_limit;
} acpi_block_entry_t;

#define ACPI_INTERP_STATE_POPULATE_STACKITEM 1
#define ACPI_INTERP_STATE_METHOD_STACKITEM 2
#define ACPI_INTERP_STATE_LOOP_STACKITEM 3
#define ACPI_INTERP_STATE_COND_STACKITEM 4
#define ACPI_INTERP_STATE_BUFFER_STACKITEM 5
#define ACPI_INTERP_STATE_PACKAGE_STACKITEM 6
#define ACPI_INTERP_STATE_NODE_STACKITEM 7 
#define ACPI_INTERP_STATE_OP_STACKITEM 8 
#define ACPI_INTERP_STATE_INVOKE_STACKITEM 9
#define ACPI_INTERP_STATE_RETURN_STACKITEM 10 
#define ACPI_INTERP_STATE_BANKFIELD_STACKITEM 11
#define ACPI_INTERP_STATE_VARPACKAGE_STACKITEM 12

typedef struct {
  int type;
  int opstack_frame;

  union {
    struct {
      uint8_t method_result_requested;
    } method;
    struct {
      int cond_state;
      int cond_has_else;
      int cond_else_pc;
      int cond_else_limit;
    } condition;
    struct {
      int loop_state;
      int loop_predicate_pc;
    } loop;
    struct {
      uint8_t buffer_result_requested;
    } buffer;
    struct {
      int pkg_idx;

#define ACPI_PKG_PHASE_PARSE_SIZE 0
#define ACPI_PKG_PHASE_CREATE_OBJ 1
#define ACPI_PKG_PHASE_ENUMERATE_ITEMS 2

      int pkg_phase;

      uint8_t pkg_result_requested;
    } pkg;
    struct {
      int op_opcode;
      uint8_t op_arg_modes[8];
      uint8_t op_result_requested;
    } op_opcode;
    struct {
      int node_opcode_code;
      uint8_t node_arg_modes[8];
    } node_opcode;
    struct {
      int ivk_argc;
      uint8_t ivk_result_requested;
    } invoke;
  };
} acpi_stack_entry_t;

#define INITIAL_CXT_STACK_SIZE 16
#define INITIAL_BLK_STACK_SIZE 16
#define INITIAL_STACK_SIZE 16
#define INITIAL_OPERAND_STACK_SIZE 16

typedef struct {
  acpi_context_entry_t *ctx_stack_base;
  acpi_block_entry_t *block_stack_base;
  acpi_stack_entry_t *stack_base;
  acpi_operand_t *operand_stack_base;

  uint32_t ctx_stack_max_size;
  uint32_t block_stack_max_size;
  uint32_t stack_max_size;
  uint32_t operand_stack_max_size;

  int ctx_sp;
  int block_sp;
  int stack_sp;
  int operand_sp;
} acpi_state_t;

int acpi_init_state(acpi_state_t* state);
int acpi_delete_state(acpi_state_t* state);

/*
 * acpi interpretation stack integrity functions
 */

static ALWAYS_INLINE int acpi_operand_stack_ensure_capacity(acpi_state_t* state){ 
  if ((state->operand_sp+1) > state->operand_stack_max_size) {
    // enlarge
    kernel_panic("TODO: implement acpi stack enlargement =)");
  }
  return 0;
}

static ALWAYS_INLINE int acpi_block_stack_ensure_capacity(acpi_state_t* state){ 
  if ((state->block_sp+1) > state->block_stack_max_size) {
    // enlarge
    kernel_panic("TODO: implement acpi stack enlargement =)");
  }
  return 0;
}

static ALWAYS_INLINE int acpi_context_stack_ensure_capacity(acpi_state_t* state){ 
  if ((state->ctx_sp+1) > state->ctx_stack_max_size) {
    // enlarge
    kernel_panic("TODO: implement acpi stack enlargement =)");
  }
  return 0;
}

static ALWAYS_INLINE int acpi_stack_ensure_capacity(acpi_state_t* state){ 
  if ((state->stack_sp+1) > state->stack_max_size) {
    // enlarge
    kernel_panic("TODO: implement acpi stack enlargement =)");
  }
  return 0;
}

/*
 * inline acpi stack pop functions
 */

static ALWAYS_INLINE void acpi_state_pop_stack(acpi_state_t* state) {
  if (state->stack_sp >= 0) {
    state->stack_sp--;
  }
}

static ALWAYS_INLINE void acpi_state_pop_blk_stack(acpi_state_t* state) {
  if (state->block_sp >= 0) {
    state->block_sp--;
  }
}

static ALWAYS_INLINE void acpi_state_pop_ctx_stack(acpi_state_t* state) {
  if (state->ctx_sp >= 0) {
    acpi_context_entry_t *entry = &state->ctx_stack_base[state->block_sp];

    // TODO: free the context entry

    state->ctx_sp--;
  }
}

static ALWAYS_INLINE void acpi_state_pop_opstack_n(acpi_state_t* state, int amount) {
  // if we somehow try to pop off more than we can,
  // just remove the entire opstack =)
  if (state->operand_sp <= amount) {
    amount = state->operand_sp;
  }

  for (uint32_t i = 0; i < amount; i++) {
    acpi_operand_t *operand = &state->operand_stack_base[state->operand_sp - i - 1];

    // TODO: free operand entry
  }
  state->operand_sp -= amount;
}

static ALWAYS_INLINE void acpi_state_pop_opstack(acpi_state_t* state) {
  acpi_state_pop_opstack_n(state, 1);
}

/*
 * inline acpi stack push functions
 */

// let's hope we have our memory mapped 0.0
static ALWAYS_INLINE acpi_operand_t *acpi_state_push_opstack(acpi_state_t *state) {
  if (state->operand_sp < state->operand_stack_max_size) {
    acpi_operand_t *object = &state->operand_stack_base[state->operand_sp];
    memset(object, 0, sizeof(acpi_operand_t));
    state->operand_sp++;
    return object;
  }
  return nullptr;
}

static ALWAYS_INLINE acpi_stack_entry_t *acpi_state_push_stack_entry(acpi_state_t* state) {
  if ((state->stack_sp+1) < state->stack_max_size) {

    state->stack_sp++;
    return (acpi_stack_entry_t*)&state->stack_base[state->stack_sp];
  }
  return nullptr;
}

static ALWAYS_INLINE acpi_block_entry_t *acpi_state_push_block_entry(acpi_state_t* state) {
  if ((state->block_sp+1) < state->block_stack_max_size) {
    state->block_sp++;
    acpi_block_entry_t* entry = &state->block_stack_base[state->block_sp];
    memset(entry, 0x00, sizeof(acpi_block_entry_t));
    return entry;
  }
  return nullptr;
}

static ALWAYS_INLINE acpi_context_entry_t *acpi_state_push_context_entry(acpi_state_t* state) {
  if ((state->ctx_sp+1) < state->ctx_stack_max_size) {
    state->ctx_sp++;
    acpi_context_entry_t* entry = &state->ctx_stack_base[state->ctx_sp];
    memset(entry, 0x00, sizeof(acpi_context_entry_t));
    return entry;
  }
  return nullptr;
}

/*
 * inline acpi stack peek functions
 */

static ALWAYS_INLINE acpi_operand_t* acpi_operand_stack_peek(acpi_state_t* state) {
  if (state->operand_sp >= 0) {
    return &state->operand_stack_base[state->operand_sp];
  }
  return nullptr;
}

static ALWAYS_INLINE acpi_block_entry_t* acpi_block_stack_peek(acpi_state_t* state) {
  if (state->block_sp >= 0) {
    return &state->block_stack_base[state->block_sp];
  }
  return nullptr;
}

static ALWAYS_INLINE acpi_context_entry_t* acpi_context_stack_peek(acpi_state_t* state) {
  if (state->ctx_sp >= 0) {
    return &state->ctx_stack_base[state->ctx_sp];
  }
  return nullptr;
}

static ALWAYS_INLINE acpi_stack_entry_t* acpi_stack_peek(acpi_state_t* state) {
  if (state->stack_sp >= 0) {
    return &state->stack_base[state->stack_sp];
  }
  return nullptr;
}

static ALWAYS_INLINE acpi_stack_entry_t* acpi_stack_peek_at(acpi_state_t* state, uint32_t index) {
  if (index >= 0) {
    return &state->stack_base[index];
  }
  return nullptr;
}


static ALWAYS_INLINE void acpi_drain_block_stack(acpi_state_t* state) {
  while (state->block_sp >= 0) {
    acpi_state_pop_blk_stack(state);
  }
}

static ALWAYS_INLINE void acpi_drain_contex_stack(acpi_state_t* state) {
  while (state->ctx_sp >= 0) {
    acpi_state_pop_ctx_stack(state);
  }
}

static ALWAYS_INLINE void acpi_drain_stack(acpi_state_t* state) {
  while (state->stack_sp >= 0) {
    acpi_state_pop_stack(state);
  }
}

void acpi_load_package(acpi_variable_t* package, acpi_variable_t* buffer, size_t i); // TODO
void acpi_store_package(acpi_variable_t* package, acpi_variable_t* var, size_t i); // TODO

void init_acpi_var(acpi_variable_t* var); // TODO
void destroy_acpi_var(acpi_variable_t* var);

static ALWAYS_INLINE void swap_acpi_var(acpi_variable_t* one, acpi_variable_t* two) {
  acpi_variable_t buffer = *one;
  *one = *two;
  *two = buffer;
}

void move_acpi_var(acpi_variable_t* dst, acpi_variable_t* src);
void assign_acpi_var(acpi_variable_t* from, acpi_variable_t* to);

void acpi_load_from_ns_node(acpi_variable_t* var, struct acpi_ns_node* node); // TODO

void acpi_write_buffer(struct acpi_ns_node* buffer, acpi_variable_t* src); // TODO
void acpi_read_buffer(acpi_variable_t *dst, struct acpi_ns_node* buffer); // TODO

/*
 * Operand functions
 */
void acpi_operand_load(acpi_state_t* state, acpi_operand_t* operand, acpi_variable_t* buffer); // TODO


#endif //__ANIVA_ACPI_OBJ__
