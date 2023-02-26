#include "acpi_obj.h"
#include "libk/reference.h"
#include "mem/PagingComplex.h"
#include "mem/heap.h"
#include <libk/string.h>

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

int acpi_var_create_and_init_str(acpi_variable_t* var, const char* str) {
  int result = acpi_var_create_str(var, strlen(str));
  if (result < 0) {
    return result;
  }

  memcpy(var->str_p->str, str, strlen(str));
  return 0;
}

int acpi_var_create_str(acpi_variable_t* var, size_t length) {
  var->var_type = ACPI_STRING;

  acpi_str_t *str_obj = kmalloc(sizeof(acpi_str_t));

  if (!str_obj) {
    return -1;
  }

  str_obj->str = kmalloc(length + 1);
  str_obj->rc = 1;
  str_obj->size = length + 1;

  if (!str_obj->str) {
    kfree(str_obj);
    return -1;
  }

  var->str_p = str_obj;

  return 0;
}

int acpi_var_create_buffer(acpi_variable_t* var, size_t length) {
  var->var_type = ACPI_BUFFER;
  var->buffer_p = kmalloc(sizeof(acpi_buffer_t));
  var->buffer_p->buffer = kmalloc(length);
  var->buffer_p->size = length;
  var->buffer_p->rc = 1;
  memset(var->buffer_p->buffer, 0x00, length);
  return 0;
}

int acpi_var_create_package(acpi_variable_t* var, size_t length) {
  var->var_type = ACPI_PACKAGE;
  var->package_p = kmalloc(sizeof(acpi_package_t));
  var->package_p->vars = kmalloc(sizeof(acpi_variable_t) * length);
  var->package_p->pkg_size = length;
  var->package_p->rc = 1;
  memset(var->package_p->vars, 0x00, length * sizeof(acpi_variable_t));
  return 0;
}

int acpi_var_resize_str(acpi_variable_t* var, size_t length) {
  if (var->var_type != ACPI_TYPE_STRING) {
    return -1;
  }

  // TODO: downscale?
  if (length > strlen(var->str_p->str)) {
    char* new_str_buff = kmalloc(length + 1);
    strcpy(new_str_buff, var->str_p->str);
    kfree(var->str_p->str);
    var->str_p->str = new_str_buff;
    var->str_p->size = length + 1;
    return 0;
  }
  return -2;
}

int acpi_var_resize_buffer(acpi_variable_t* var, size_t length) {
  if (var->var_type != ACPI_BUFFER) {
    return -1;
  }

  if (length > var->buffer_p->size) {
    uint8_t* new_buffer = kmalloc(length);
    memset(new_buffer, 0x00, length);

    memcpy(var->buffer_p->buffer, new_buffer, var->buffer_p->size);
    kfree(var->buffer_p->buffer);

    var->buffer_p->buffer = new_buffer;
    var->buffer_p->size = length;
    return 0;
  }
  return -2;
}

int acpi_var_resize_package(acpi_variable_t* var, size_t length) {
  if (var->var_type != ACPI_PACKAGE) {
    return -1;
  }

  if (length > var->package_p->pkg_size) {
    acpi_variable_t* new_vars = kmalloc(length * sizeof(acpi_variable_t));
    for (uint32_t i = 0; i < var->package_p->pkg_size; i++) {
      move_acpi_var(&new_vars[i], &var->package_p->vars[i]);
    }
    kfree(var->package_p->vars);
    var->package_p->vars = new_vars;
    var->package_p->pkg_size = length;
    return 0;
  }

  for (uint32_t i = length; i < var->package_p->pkg_size; i++) {
    destroy_acpi_var(&var->package_p->vars[i]);
  }
  var->package_p->pkg_size = length;
  return 0;
}
