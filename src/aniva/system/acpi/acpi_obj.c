#include "acpi_obj.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/reference.h"
#include "mem/pg.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "system/acpi/namespace.h"
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
  kernel_panic("TODO: init_acpi_var");

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

void acpi_load_integer(acpi_state_t* state, acpi_operand_t* src, acpi_variable_t* dest) {
  ASSERT_MSG(src->tag == ACPI_OPERAND_OBJECT, "Passed a non-object to acpi_load_integer");

  acpi_variable_t data = {0};
  assign_acpi_var(&src->obj, &data);

  ASSERT_MSG(data.var_type == ACPI_INTEGER, "Failed to load integer");
  move_acpi_var(dest, &data);
}

void acpi_load_package(acpi_variable_t* package, acpi_variable_t* buffer, size_t i) {
  ASSERT_MSG(package->var_type == ACPI_PACKAGE, "Tried to call acpi_store_package on a non-package object");

  assign_acpi_var(&package->package_p->vars[i], buffer);
}
void acpi_store_package(acpi_variable_t* package, acpi_variable_t* var, size_t i) {
  ASSERT_MSG(package->var_type == ACPI_PACKAGE, "Tried to call acpi_store_package on a non-package object");

  assign_acpi_var(var, &package->package_p->vars[i]);
}

void acpi_store_namespace(struct acpi_ns_node* node, acpi_variable_t* object) {
  kernel_panic("TODO: implement acpi_store_namespace");
}

void acpi_mutate_namespace(acpi_ns_node_t* node, acpi_variable_t* obj) {
  switch (node->type) {
    case ACPI_NAMESPACE_NAME:
      switch (node->object.var_type) {
        case ACPI_INTEGER:
          ASSERT_MSG(obj->var_type == ACPI_INTEGER, "Failed to mutate namespace");
          node->object.num = obj->num;
          break;
        case ACPI_STRING:
          // TODO: refc?
          memset(node->object.str_p->str, 0, node->object.str_p->size);
          memcpy(node->object.str_p->str, obj->str_p->str, obj->str_p->size);
          node->object.str_p->size = obj->str_p->size;
          break;
        case ACPI_BUFFER:
          // TODO: refc?
          memset(node->object.buffer_p->buffer, 0, node->object.buffer_p->size);
          memcpy(node->object.buffer_p->buffer, obj->buffer_p->buffer, obj->buffer_p->size);
          node->object.buffer_p->size = obj->buffer_p->size;
        case ACPI_PACKAGE:
          ASSERT_MSG(obj->var_type == ACPI_PACKAGE, "Failed to mutate namespace");
          size_t pkg_size = obj->package_p->pkg_size;
          if (acpi_var_resize_package(&node->object, pkg_size) < 0) {
            kernel_panic("failed to resize ACPI package");
          }
          for (uintptr_t i = 0; i < pkg_size; i++) {
            acpi_variable_t temp = {0};
            acpi_load_package(&temp, obj, i);
            acpi_store_package(&temp, &node->object, i);
          }

        default:
          break;
      }
      break;
    case ACPI_NAMESPACE_FIELD:
    case ACPI_NAMESPACE_INDEXFIELD:
    case ACPI_NAMESPACE_BANKFIELD:
    case ACPI_NAMESPACE_BUFFER_FIELD:
      kernel_panic("TODO: implement other acpi namespace mutations");
      break;
  }
}

void acpi_load_object(acpi_variable_t* dest, struct acpi_ns_node* node) {
  switch (node->type) {
    case ACPI_NAMESPACE_NAME:
      assign_acpi_var(&node->object, dest);
      break;
    case ACPI_NAMESPACE_FIELD:
    case ACPI_NAMESPACE_INDEXFIELD:
    case ACPI_NAMESPACE_BANKFIELD:
      kernel_panic("TODO: implement reading opregions");
    case ACPI_NAMESPACE_BUFFER_FIELD:
      acpi_read_buffer(dest, node);
      break;
    case ACPI_NAMESPACE_EVENT:
    case ACPI_NAMESPACE_MUTEX:
    case ACPI_NAMESPACE_OPREGION:
    case ACPI_NAMESPACE_THERMALZONE:
    case ACPI_NAMESPACE_PROCESSOR:
    case ACPI_NAMESPACE_POWERRESOURCE:
    case ACPI_NAMESPACE_DEVICE:
      dest->var_type = ACPI_HANDLE;
      dest->handle = node;
      break;
    default:
      kernel_panic("(acpi_load_object) unknown type");
  }
}

void acpi_mutate_operand(acpi_state_t* state, acpi_operand_t* dest, acpi_variable_t* src) {
  switch (dest->tag) {
    case ACPI_OPERAND_OBJECT: {
      if (dest->obj.var_type == ACPI_STRING_INDEX) {
        char* window = dest->obj.str_p->str;
        window[dest->obj.num] = src->num;
        return;
      }
      if (dest->obj.var_type == ACPI_BUFFER_INDEX) {
        uint8_t* window = dest->obj.buffer_p->buffer;
        window[dest->obj.num] = src->num;
        return;
      }
      if (dest->obj.var_type == ACPI_PACKAGE_INDEX) {

        acpi_variable_t package = {0};
        assign_acpi_var(src, &package);
        acpi_store_package(&package, dest->obj.package_p->vars, dest->obj.num);
        destroy_acpi_var(&package);
        return;
      }
      kernel_panic("acpi_mutate_operand: Unknown operand object");
      return;
    }
    case ACPI_NULL_NAME:
      break;
    case ACPI_RESOLVED_NAME:
      acpi_mutate_namespace(dest->handle, src);
      break;
    case ACPI_ARG_NAME:
      ASSERT_MSG(acpi_context_stack_peek(state)->invocation != nullptr, "Tried to mutate arg name without an invocation");
      acpi_context_entry_t* context = acpi_context_stack_peek(state);

      acpi_variable_t *arg_var = &context->invocation->args[dest->idx];
      switch (arg_var->var_type) {
        case ACPI_ARG_REF:
          assign_acpi_var(src, &arg_var->invocation_p.ptr->args[arg_var->invocation_p.ref_index]);
          break;
        case ACPI_LOCAL_REF:
          assign_acpi_var(src, &arg_var->invocation_p.ptr->local[arg_var->invocation_p.ref_index]);
          break;
        case ACPI_NODE_REF:
          acpi_store_namespace(arg_var->handle, src);
          break;
        default:
          assign_acpi_var(src, arg_var);
          break;
      }
      break;
    case ACPI_LOCAL_NAME:
      ASSERT_MSG(acpi_context_stack_peek(state)->invocation != nullptr, "Tried to mutate arg name without an invocation");
      context = acpi_context_stack_peek(state);
      assign_acpi_var(src, &context->invocation->local[dest->idx]);
      break;
    case ACPI_DEBUG_NAME:
      println("ACPI: hit ACPI_DEBUG_NAME");
      break;
    default:
      kernel_panic("Unknown tag for mutating operand");
  }
}

void move_acpi_var(acpi_variable_t* dst, acpi_variable_t* src) {

  acpi_variable_t buffer = {0};

  swap_acpi_var(&buffer, src);
  swap_acpi_var(&buffer, dst);

  destroy_acpi_var(&buffer);
}

// ->one is where we copy from
// ->two is where we copy to
void assign_acpi_var(acpi_variable_t* from, acpi_variable_t* to) {

  acpi_variable_t var = *from;
  switch (from->var_type) {
    CASE_ACPI_STRING;
      flat_ref(&from->str_p->rc);
      break;

    CASE_ACPI_BUFFER;
      flat_ref(&from->buffer_p->rc);
      break;

    CASE_ACPI_PACKAGE;
      flat_ref(&from->package_p->rc);
      break;
  }

  move_acpi_var(to, &var);
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

void acpi_get_obj_ref(acpi_state_t* state, acpi_operand_t* op, acpi_variable_t* obj) {
  ASSERT_MSG(op->tag == ACPI_OPERAND_OBJECT, "acpi_get_obj_ref: operand is not an object");
  assign_acpi_var(&op->obj, obj);
}

// credit to https://github.com/managarm/lai/blob/master/core/exec-operand.c
void acpi_write_buffer(acpi_ns_node_t* buffer, acpi_variable_t* src) {
  uint64_t value = src->num;

  // Offset that we are writing to, in bytes.
  size_t offset = buffer->namespace_buffer_field.bf_offset_bits;
  size_t size = buffer->namespace_buffer_field.bf_size_bits;
  uint8_t *data = buffer->namespace_buffer_field.bf_buffer->buffer;

  size_t n = 0; // Number of bits that have been written.
  while (n < size) {
    // First bit (of the current byte) that will be overwritten.
    int bit = (offset + n) & 7;

    // Number of bits (of the current byte) that will be overwritten.
    int m = size - n;
    if (m > (8 - bit))
        m = 8 - bit;

    uint8_t mask = (1 << m) - 1;
    data[(offset + n) >> 3] &= ~(mask << bit);
    data[(offset + n) >> 3] |= ((value >> n) & mask) << bit;

    n += m;
  }
}

// credit to https://github.com/managarm/lai/blob/master/core/exec-operand.c
void acpi_read_buffer(acpi_variable_t *dst, acpi_ns_node_t* buffer) {
  size_t offset = buffer->namespace_buffer_field.bf_offset_bits;
  size_t size = buffer->namespace_buffer_field.bf_size_bits;
  uint8_t *data = buffer->namespace_buffer_field.bf_buffer->buffer;

  dst->var_type = ACPI_INTEGER;
  dst->num = 0;

  size_t n = 0;
  while (n < size) {
      int bit = (offset + n) & 7;
      int m = size - n;
      if (m > (8 - bit))
          m = 8 - bit;

      uint8_t mask = (1 << m) - 1;
      uint8_t cur_byte = data[(offset + n) >> 3];
      uint8_t to_write = ((cur_byte & mask));

      dst->num |= (uint64_t)to_write << n;
      n += m;
  }
}

void acpi_operand_load(acpi_state_t* state, acpi_operand_t* operand, acpi_variable_t* buffer) {

  if (operand->tag == ACPI_ARG_NAME) {
    acpi_context_entry_t* context = acpi_context_stack_peek(state);
    ASSERT_MSG(context->invocation != nullptr, "ACPI_ARG_NAME did not have an invocation!");
    assign_acpi_var(&context->invocation->args[operand->idx], buffer);
    return;
  } else if (operand->tag == ACPI_LOCAL_NAME) {
    acpi_context_entry_t* context = acpi_context_stack_peek(state);
    ASSERT_MSG(context->invocation != nullptr, "ACPI_LOCAL_NAME did not have an invocation!");
    assign_acpi_var(&context->invocation->local[operand->idx], buffer);
    return;
  } else if(operand->tag == ACPI_RESOLVED_NAME) {
    acpi_load_object(buffer, operand->handle);
    return;
  }
  kernel_panic("Assert not reached");
}
