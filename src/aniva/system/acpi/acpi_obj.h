#ifndef __ANIVA_ACPI_OBJ__
#define __ANIVA_ACPI_OBJ__
#include <libk/stddef.h>
#include "libk/linkedlist.h"
#include "structures.h"
#include "libk/string.h"

struct acpi_str;
struct acpi_buffer;
struct acpi_package;
struct acpi_operand;

typedef struct {

} acpi_object_t;

typedef struct {
  int var_type;
  uint64_t num;

  union {
    struct acpi_str* str_p;
    struct acpi_buffer* buffer_p;
    struct acpi_package* package_p;
  };



  int var_idx;
} acpi_variable_t;

typedef struct acpi_str {

} acpi_str_head_t;

typedef struct acpi_buffer {

} acpi_buffer_head_t;

typedef struct acpi_package {

} acpi_package_t;


typedef struct acpi_operand {

} acpi_operand_t;

/*
 * acpi invocation
 */
typedef struct {
  // an invocation has a maximum of 7 arguments
  acpi_variable_t args[7];
  // with 8 locally
  acpi_variable_t local[8];

  list_t *resulting_ns_nodes;
} acpi_invocation_t;

typedef struct {

} acpi_context_entry_t;

typedef struct {

} acpi_block_entry_t;

typedef struct {

} acpi_stack_entry_t;

typedef struct {
  acpi_context_entry_t *cxt_stack_base;
  acpi_block_entry_t *blk_stack_base;
  acpi_stack_entry_t *stack_base;
  acpi_operand_t *operand_stack_base;

  uint32_t cxt_stack_max_size;
  uint32_t block_stack_max_size;
  uint32_t stack_max_size;
  uint32_t operand_stack_max_size;

  int cxt_sp;
  int block_sp;
  int stack_sp;
  int operand_sp;
} acpi_state_t;

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
  if (state->cxt_sp >= 0) {
    acpi_context_entry_t *entry = &state->cxt_stack_base[state->block_sp];

    // TODO: free the context entry

    state->cxt_sp--;
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
}

static ALWAYS_INLINE acpi_block_entry_t *acpi_state_push_block_entry(acpi_state_t* state) {
  // TODO
}

static ALWAYS_INLINE acpi_context_entry_t *acpi_state_push_context_entry(acpi_state_t* state) {
  // TODO
}

void init_acpi_var(acpi_variable_t* var);
void destroy_acpi_var(acpi_variable_t* var);
void move_acpi_var(acpi_variable_t* dst, acpi_variable_t* src);
void assign_acpi_var(acpi_variable_t* one, acpi_variable_t* two);

#endif //__ANIVA_ACPI_OBJ__
