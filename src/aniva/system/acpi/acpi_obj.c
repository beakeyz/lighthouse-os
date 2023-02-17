#include "acpi_obj.h"
#include "libk/reference.h"
#include "mem/heap.h"

int acpi_init_state(acpi_state_t* state) {
  memset(state, 0, sizeof(*state));

  state->operand_sp = -1;
  state->block_sp = -1;
  state->ctx_sp = -1;
  state->stack_sp = -1;
  
  state->operand_stack_max_size = INITIAL_OPERAND_STACK_SIZE;
  state->block_stack_max_size = INITIAL_BLK_STACK_SIZE;
  state->ctx_stack_max_size = INITIAL_CXT_STACK_SIZE;
  state->stack_max_size = INITIAL_STACK_SIZE;

  // TODO: either allocate on the heap,
  // or have some minimal-sized stack-allocated
  // array that we can expand ondemand?

  state->operand_stack_base = kmalloc(state->operand_stack_max_size * sizeof(acpi_operand_t));
  state->block_stack_base = kmalloc(state->block_stack_max_size * sizeof(acpi_block_entry_t));
  state->ctx_stack_base = kmalloc(state->block_stack_max_size * sizeof(acpi_context_entry_t));
  state->stack_base = kmalloc(state->stack_max_size * sizeof(acpi_stack_entry_t));

  return 0;
}

int acpi_delete_state(acpi_state_t* state) {

  acpi_state_pop_opstack_n(state, state->operand_sp);

  acpi_drain_block_stack(state);
  acpi_drain_contex_stack(state);
  acpi_drain_stack(state);

  kfree(state->operand_stack_base);
  kfree(state->block_stack_base);
  kfree(state->ctx_stack_base);
  kfree(state->stack_base);

  return 0;
}

#define CASE_ACPI_STRING \
  case ACPI_STRING:        \
  case ACPI_STRING_INDEX:  \

#define CASE_ACPI_BUFFER \
  case ACPI_BUFFER:        \
  case ACPI_BUFFER_INDEX:  \

#define CASE_ACPI_PACKAGE \
  case ACPI_PACKAGE:        \
  case ACPI_PACKAGE_INDEX:  \

void init_acpi_var(acpi_variable_t* var) {

  // TODO

}

void destroy_acpi_var(acpi_variable_t* var) {

  if (!var)
    return;

  switch (var->var_type) {
    CASE_ACPI_STRING;
      flat_unref(&var->str_p->rc);
      if (var->str_p->rc == 0) {
        kfree(var->str_p->str);
        kfree(var->str_p);
      }
      break;

    CASE_ACPI_BUFFER;
      flat_unref(&var->buffer_p->rc);
      if (var->buffer_p->rc == 0) {
        kfree(var->buffer_p->buffer);
        kfree(var->buffer_p);
      }
      break;

    CASE_ACPI_PACKAGE;
      flat_unref(&var->package_p->rc);
      if (var->package_p->rc == 0) {
        for (uintptr_t i = 0; i < var->package_p->pkg_size; i++) {
          destroy_acpi_var(&var->package_p->vars[i]);
        }
        kfree(var->package_p->vars);
        kfree(var->package_p);
      }
      break;
  }

  memset(var, 0, sizeof(acpi_variable_t));
}

void move_acpi_var(acpi_variable_t* dst, acpi_variable_t* src) {

  acpi_variable_t buffer = {0};

  swap_acpi_var(dst, &buffer);
  swap_acpi_var(src, &buffer);

  destroy_acpi_var(&buffer);
}

// ->one is where we copy from
// ->two is where we copy to
void assign_acpi_var(acpi_variable_t* one, acpi_variable_t* two) {

  acpi_variable_t var = *one;
  switch (one->var_type) {
    CASE_ACPI_STRING;
      flat_ref(&one->str_p->rc);
      break;

    CASE_ACPI_BUFFER;
      flat_ref(&one->buffer_p->rc);
      break;

    CASE_ACPI_PACKAGE;
      flat_ref(&one->package_p->rc);
      break;
  }

  move_acpi_var(two, &var);
}
