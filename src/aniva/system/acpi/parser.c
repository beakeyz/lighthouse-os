#include "parser.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "libk/multiboot.h"
#include "libk/reference.h"
#include "mem/PagingComplex.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "system/acpi/namespace.h"
#include "system/acpi/opcodes.h"
#include "system/acpi/structures.h"
#include <libk/stddef.h>
#include <libk/string.h>
#include "acpi_obj.h"

acpi_parser_t *g_parser_ptr;

#define RSDP_SIGNATURE "RSD PTR "
#define FADT_SIGNATURE "FACP"
#define DSDT_SIGNATURE "DSDT"
#define MADT_SIGNATURE "APIC"
#define SSDT_SIGNATURE "SSDT"
#define PSDT_SIGNATURE "PSDT"

static size_t parse_acpi_var_int(size_t* out, uint8_t *code_ptr, int* pc, int limit);
static ALWAYS_INLINE int acpi_parse_u8(uint8_t* out, uint8_t* code_ptr, int* pc, int limit);
static ALWAYS_INLINE int acpi_parse_u16(uint16_t* out, uint8_t* code_ptr, int* pc, int limit);
static ALWAYS_INLINE int acpi_parse_u32(uint32_t* out, uint8_t* code_ptr, int* pc, int limit);
static ALWAYS_INLINE int acpi_parse_u64(uint64_t* out, uint8_t* code_ptr, int* pc, int limit);

void init_acpi_parser(acpi_parser_t* parser, uintptr_t multiboot_addr) {
  g_parser_ptr = parser;

  size_t table_index;
  acpi_aml_seg_t* aml_segment;
  acpi_aml_table_t* aml_table;
  void* dsdt_table;
  acpi_rsdp_t* rsdp;
  uint32_t hw_reduced;

  println("Starting ACPI parser");

  parser->m_multiboot_addr = multiboot_addr;

  rsdp = find_rsdp(parser);

  if (rsdp == nullptr) {
    // FIXME: we're fucked lol
    kernel_panic("no rsdt found =(");
  }

  parser->m_rsdp = rsdp;
  parser->m_is_xsdp = (rsdp->revision >= 2);

  parser->m_fadt = find_table(parser, FADT_SIGNATURE);
  hw_reduced = ((parser->m_fadt->flags >> 20) & 1);

  if (!parser->m_fadt) {
    kernel_panic("Unable to find ACPI table FADT!");
  }

  print_tables(rsdp);

  // FIXME: fix the acpi (AML) parser ;-;
  parser->m_ns_root_node = acpi_create_root();

  dsdt_table = (void*)(uintptr_t)parser->m_fadt->dsdt_ptr;
  //if (parser->m_fadt->x_dsdt) {
  //  dsdt_table = (void*)(uintptr_t)parser->m_fadt->x_dsdt;
  //}

  if (!dsdt_table)
    kernel_panic("Unable to find ACPI table DSDT!");

  aml_segment = acpi_load_segment(dsdt_table, 0);

  acpi_init_state(&parser->m_state);
  parser_prepare_acpi_state(&parser->m_state, aml_segment, parser->m_ns_root_node);
  acpi_delete_state(&parser->m_state);

  table_index = 0;
  aml_segment = nullptr;
  aml_table = nullptr;

  println("ACPI_PARSER: loading SSDT");

  while ((aml_table = find_table_idx(parser, SSDT_SIGNATURE, table_index))) {
    aml_segment = acpi_load_segment(aml_table, table_index);

    acpi_init_state(&parser->m_state);
    parser_prepare_acpi_state(&parser->m_state, aml_segment, parser->m_ns_root_node);
    acpi_delete_state(&parser->m_state);

    table_index++;
  }

  table_index = 0;
  aml_segment = nullptr;
  aml_table = nullptr;

  println("ACPI_PARSER: loading PSDT");

  while ((aml_table = find_table_idx(parser, PSDT_SIGNATURE, table_index))) {
    aml_segment = acpi_load_segment(aml_table, table_index);

    acpi_init_state(&parser->m_state);
    parser_prepare_acpi_state(&parser->m_state, aml_segment, parser->m_ns_root_node);
    acpi_delete_state(&parser->m_state);

    table_index++;
  }

  println(to_string(parser->m_ns_size));
  for (uintptr_t i = 0; i < parser->m_ns_size; i++) {
    char name[5];
    name[0] = parser->m_ns_nodes[i]->name & 0xFF;
    name[1] = (parser->m_ns_nodes[i]->name >> 8) & 0xFF;
    name[2] = (parser->m_ns_nodes[i]->name >> 16) & 0xFF;
    name[3] = (parser->m_ns_nodes[i]->name >> 24) & 0xFF;
    name[4] = '\0';
    println(name);

    for (uintptr_t j = 0; j < parser->m_ns_nodes[i]->m_children_count; j++) {
      acpi_ns_node_t* child = parser->m_ns_nodes[i]->m_children[j];

      name[0] = child->name & 0xFF;
      name[1] = (child->name >> 8) & 0xFF;
      name[2] = (child->name >> 16) & 0xFF;
      name[3] = (child->name >> 24) & 0xFF;
      name[4] = '\0';
      print(" -- ");
      println(name);

      for (uintptr_t j = 0; j < child->m_children_count; j++) {
        acpi_ns_node_t* child_child = child->m_children[j];

        name[0] = child_child->name & 0xFF;
        name[1] = (child_child->name >> 8) & 0xFF;
        name[2] = (child_child->name >> 16) & 0xFF;
        name[3] = (child_child->name >> 24) & 0xFF;
        name[4] = '\0';
        print(" -- -- ");
        println(name);

        for (uintptr_t j = 0; j < child_child->m_children_count; j++) {
          acpi_ns_node_t* child_child_child = child_child->m_children[j];

          name[0] = child_child_child->name & 0xFF;
          name[1] = (child_child_child->name >> 8) & 0xFF;
          name[2] = (child_child_child->name >> 16) & 0xFF;
          name[3] = (child_child_child->name >> 24) & 0xFF;
          name[4] = '\0';
          print(" -- -- ");
          println(name);

        }
      }
    }
  }

  // TODO:
}

void* find_rsdp(acpi_parser_t* parser) {
  const char* rsdt_sig = "RSD PTR ";
  // TODO: check other spots

  // check multiboot header
  struct multiboot_tag_new_acpi* new_ptr = get_mb2_tag((void*)parser->m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_NEW);

  if (new_ptr && new_ptr->rsdp[0]) {
    void* ptr = kmem_kernel_alloc((uintptr_t)new_ptr->rsdp, sizeof(uintptr_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
    print("Multiboot has xsdp: ");
    println(to_string((uintptr_t)ptr));
    return ptr;
  }

  struct multiboot_tag_old_acpi* old_ptr = get_mb2_tag((void*)parser->m_multiboot_addr, MULTIBOOT_TAG_TYPE_ACPI_OLD);

  if (old_ptr && old_ptr->rsdp[0]) {
    void* ptr = kmem_kernel_alloc((uintptr_t)old_ptr->rsdp, sizeof(uintptr_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
    print("Multiboot has rsdp: ");
    println(to_string((uintptr_t)ptr));
    return ptr;
  }

  // check bios mem
  const uintptr_t bios_start_addr = 0xe0000;
  const size_t bios_mem_size = ALIGN_UP(128 * Kib, SMALL_PAGE_SIZE);

  uintptr_t ptr = (uintptr_t)kmem_kernel_alloc(bios_start_addr, bios_mem_size, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  if (ptr != NULL) {
    for (uintptr_t i = ptr; i < ptr + bios_mem_size; i+=16) {
      void* potential = (void*)i;
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        return potential;
      }
    }
  }

  const list_t* phys_ranges = kmem_get_phys_ranges_list();
  FOREACH(i, phys_ranges) {

    phys_mem_range_t *range = i->data;

    if (range->type != PMRT_ACPI_NVS || range->type != PMRT_ACPI_RECLAIM) {
      continue;
    }

    uintptr_t start = range->start;
    uintptr_t length = range->length;

    for (uintptr_t i = start; i < (start + length); i += 16) {
      void *potential = (void *) i;
      if (memcmp(rsdt_sig, potential, strlen(rsdt_sig))) {
        return potential;
      }
    }
  }

  return nullptr;
}

// find a table by poking in memory. 
// TODO: find a way to cache the memory locations for all the tables
void* find_table_idx(acpi_parser_t *parser, const char* sig, size_t index) {

  void* rsdp_addr = parser->m_rsdp;

  if (rsdp_addr == nullptr) {
    return nullptr;
  }

  if (strlen(sig) != 4) {
    return nullptr;
  }

  acpi_xsdp_t* ptr = kmem_kernel_alloc((uintptr_t)rsdp_addr, sizeof(acpi_xsdp_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  if (ptr->base.revision >= 2) {
    // xsdt
    if (ptr->xsdt_addr) {
      acpi_xsdt_t* xsdt = kmem_kernel_alloc((uintptr_t)ptr->xsdt_addr, sizeof(acpi_xsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

      if (xsdt != nullptr) {
        const size_t end_index = ((xsdt->base.length - sizeof(acpi_sdt_header_t))) / sizeof(uint64_t);

        for (uint64_t i = 0; i < end_index; i++) {
          acpi_rsdt_t* cur = (acpi_rsdt_t*)kmem_kernel_alloc((uintptr_t)xsdt->tables[i], sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
          if (memcmp(cur->base.signature, sig, 4) != 0) {
            if (index == 0) {
              return (void*)(uintptr_t)xsdt->tables[i];
            }
            index--;
          }
        }
      }
    } 
  }

  acpi_rsdt_t* rsdt = kmem_kernel_alloc((uintptr_t)ptr->base.rsdt_addr, sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  const size_t end_index = ((rsdt->base.length - sizeof(acpi_sdt_header_t))) / sizeof(uint32_t);

  for (uint32_t i = 0; i < end_index; i++) {
    acpi_rsdt_t* cur = (acpi_rsdt_t*)kmem_kernel_alloc((uintptr_t)rsdt->tables[i], sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
    if (memcmp(cur->base.signature, sig, 4) != 0) {
      if (index == 0) {
        return (void*)(uintptr_t)rsdt->tables[i];
      }
      index--;
    }
  }
  
  return nullptr;
}

void* find_table(acpi_parser_t *parser, const char* sig) {
  return (void*)find_table_idx(parser, sig, 0);
}

void print_tables(void* rsdp_addr) {
  acpi_xsdp_t* ptr = kmem_kernel_alloc((uintptr_t)rsdp_addr, sizeof(acpi_xsdp_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  if (ptr->base.revision >= 2 && ptr->xsdt_addr) {
    // xsdt
    kernel_panic("TODO: implement higher revisions >=(");
  }

  acpi_rsdt_t* rsdt = kmem_kernel_alloc((uintptr_t)ptr->base.rsdt_addr, sizeof(acpi_rsdt_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  const size_t end_index = ((rsdt->base.length - sizeof(acpi_sdt_header_t))) / sizeof(uint32_t);

  for (uint32_t i = 0; i < end_index; i++) {
    acpi_rsdt_t* cur = (acpi_rsdt_t*)(uintptr_t)rsdt->tables[i];
    for (int i = 0; i < 4; i++) {
      putch(cur->base.signature[i]);
    }
    print(" ");
  }
}

/*
 * AML stuff
 */

int parser_prepare_acpi_state(acpi_state_t* state, acpi_aml_seg_t* segment, acpi_ns_node_t* parent_node) {

  // TODO: make sure our state-stacks have enough memory

  const size_t data_size = segment->aml_table->header.length - sizeof(acpi_sdt_header_t);

  println("data_soze");
  println(to_string(data_size));

  // push a context
  acpi_context_entry_t* ctx = acpi_state_push_context_entry(state);
  ctx->segm = segment;
  ctx->code = segment->aml_table->data;
  ctx->ctx_handle = parent_node;

  // push a block
  acpi_block_entry_t* block = acpi_state_push_block_entry(state);
  block->program_counter = 0;
  block->program_counter_limit = data_size;

  // push a stackitem
  acpi_stack_entry_t* stack_entry = acpi_state_push_stack_entry(state);
  stack_entry->type = ACPI_INTERP_STATE_POPULATE_STACKITEM;
  
  // TODO: evaluate/exectute aml
  int result = parser_execute_acpi_state(state);

  return result;
}

int parser_execute_acpi_state(acpi_state_t* state) {

  while (acpi_stack_peek(state)) {
    int result = parser_partial_execute_acpi_state(state);
    if (result < 0) {
      return result;
    }
  }
  return 0;
}

// TODO:
static int parser_parse_op(int opcode, acpi_state_t* state, acpi_operand_t* operands, acpi_variable_t* reduction_result) {
  acpi_variable_t result = {0};

  switch (opcode) {
    case STORE_OP:
    case COPYOBJECT_OP:
    case NOT_OP:
    case FINDSETLEFTBIT_OP:
    case FINDSETRIGHTBIT_OP:
    case CONCAT_OP:
    case ADD_OP: {
      acpi_variable_t lhs = {0};
      acpi_variable_t rhs = {0};
      acpi_load_integer(state, &operands[0], &lhs);
      acpi_load_integer(state, &operands[1], &rhs);

      result.var_type = ACPI_INTEGER;
      result.num = lhs.num + rhs.num;
      break;
    }
    case SUBTRACT_OP:
    case MOD_OP:
    case MULTIPLY_OP:
    case AND_OP:
    case OR_OP:
    case XOR_OP:
    case NAND_OP:
    case NOR_OP:
    case SHL_OP:
    case SHR_OP:
    case DIVIDE_OP:
    case INCREMENT_OP:
    case DECREMENT_OP:
    case LNOT_OP:
    case LAND_OP:
    case LOR_OP:
    case LEQUAL_OP:
    case LLESS_OP:
    case LGREATER_OP:
    case INDEX_OP:
    case MATCH_OP:
    case CONCATRES_OP:
    case DEREF_OP:
    case SIZEOF_OP:
    case REFOF_OP:
    case TOBUFFER_OP:
    case TODECIMALSTRING_OP:
    case TOHEXSTRING_OP:
    case TOINTEGER_OP:
    case TOSTRING_OP:
    case MID_OP:
    case NOTIFY_OP:
    case (EXTOP_PREFIX << 8) | CONDREF_OP:
    case (EXTOP_PREFIX << 8) | STALL_OP:
    case (EXTOP_PREFIX << 8) | SLEEP_OP:
    case (EXTOP_PREFIX << 8) | FATAL_OP:
    case (EXTOP_PREFIX << 8) | ACQUIRE_OP:
    case (EXTOP_PREFIX << 8) | RELEASE_OP:
    case (EXTOP_PREFIX << 8) | SIGNAL_OP:
    case (EXTOP_PREFIX << 8) | RESET_OP:
    case (EXTOP_PREFIX << 8) | FROM_BCD_OP:
    case (EXTOP_PREFIX << 8) | TO_BCD_OP:
    case OBJECTTYPE_OP:
    default:
      kernel_panic("(parser_parse_op): unimplemented opcode!");
      break;
  }
  return 0;
}

// TODO:
static int parser_parse_node(int opcode, acpi_state_t* state, acpi_operand_t* operands, acpi_ns_node_t* ctx_handle) {
  println(to_string(opcode));

  switch (opcode) {
    case NAME_OP: {
      acpi_variable_t obj = {0};
      acpi_assign_ref(&operands[1], &obj);

      if (operands[0].tag != ACPI_UNRESOLVED_NAME) {
        return -1;
      }
      acpi_aml_name_t aml_name = {0};
      acpi_parse_aml_name(&aml_name, operands[0].unresolved_aml.unres_aml_p);

      acpi_ns_node_t* node = acpi_create_node();
      node->type = ACPI_NAMESPACE_NAME;

      acpi_resolve_new_node(node, ctx_handle, &aml_name);
      move_acpi_var(&obj, &node->object);
      
      acpi_load_ns_node_in_parser(g_parser_ptr, node);

      acpi_context_entry_t* ctx = acpi_context_stack_peek(state);
      if (ctx->invocation) {
        //list_append(ctx->invocation, &node->per_method_item); 
      }

      break;
    }
    case BITFIELD_OP:
    case BYTEFIELD_OP:
    case WORDFIELD_OP:
    case DWORDFIELD_OP:
    case QWORDFIELD_OP:
    case (EXTOP_PREFIX << 8) | ARBFIELD_OP:
      kernel_panic("(parser_parse_node): unimplemented opcode!");
      break;
    case (EXTOP_PREFIX << 8) | OPREGION: {
      acpi_variable_t base = {0};
      base.var_type = ACPI_INTEGER;
      base.num = operands[2].obj.num;
      acpi_variable_t size = {0};
      size.var_type = ACPI_INTEGER;
      size.num = operands[3].obj.num;

      ASSERT_MSG(operands[0].tag == ACPI_UNRESOLVED_NAME, "first operand is not an UNRESOLVED_NAME");
      ASSERT_MSG(operands[1].tag == ACPI_OPERAND_OBJECT && operands[1].obj.var_type == ACPI_INTEGER, "second operand is not an OPERAND_OBJ");

      acpi_aml_name_t name = {0};
      acpi_parse_aml_name(&name, operands[0].unresolved_aml.unres_aml_p);

      println(acpi_aml_name_to_string(&name));

      acpi_ns_node_t *node = acpi_create_node();
      acpi_resolve_new_node(node, ctx_handle, &name);

      node->type = ACPI_NAMESPACE_OPREGION;
      node->namespace_opregion.op_addr_space = operands[1].obj.num;
      node->namespace_opregion.op_base = base.num;
      node->namespace_opregion.op_length = size.num;

      acpi_load_ns_node_in_parser(g_parser_ptr, node);

      acpi_context_entry_t* entry = acpi_context_stack_peek(state);
      if (entry->invocation) {
        kernel_panic("parser_parse_node (OPREGION): this context has an invocation, which is not fully implemented!");
      }
      break;
    }
    default:
      kernel_panic("parser_parse_node: unknown opcode!");
      break;
  }
  return 0;
}

int parser_partial_execute_acpi_state(acpi_state_t* state) {

  acpi_stack_entry_t* stack = acpi_stack_peek(state);
  acpi_context_entry_t* context = acpi_context_stack_peek(state);
  acpi_block_entry_t* block = acpi_block_stack_peek(state);

  acpi_aml_seg_t* aml_segment = context->segm;
  uint8_t* code_ptr = context->code;

  acpi_invocation_t* invocation = context->invocation;
  acpi_ns_node_t* ctx_handle = context->ctx_handle;


  // ayo how thefuq did this come to be??
  if (block->program_counter > block->program_counter_limit) {
    return -1;
  }

  print("stack type: ");
  println(to_string(stack->type));

  switch (stack->type) {
    case ACPI_INTERP_STATE_POPULATE_STACKITEM:
      if (block->program_counter != block->program_counter_limit) {
        parser_set_mode(g_parser_ptr, APM_EXEC);
        return parser_parse_acpi_state(state);
      }
      acpi_state_pop_blk_stack(state);
      acpi_state_pop_ctx_stack(state);
      acpi_state_pop_stack(state);
      return 0;
    case ACPI_INTERP_STATE_METHOD_STACKITEM:
      if (block->program_counter == block->program_counter_limit) {
        if (acpi_operand_stack_ensure_capacity(state) < 0) {
          return -1;
        }

        if (stack->method.method_result_requested) {
          acpi_operand_t* op = acpi_state_push_opstack(state);
          op->tag = ACPI_OPERAND_OBJECT;
          op->obj.var_type = ACPI_INTEGER;
          op->obj.num = 0;
        }

        FOREACH(i, invocation->resulting_ns_nodes) {
          acpi_ns_node_t* node = i->data;

          if (node->type == ACPI_NAMESPACE_BUFFER_FIELD) {
            flat_unref(&node->namespace_buffer_field.bf_buffer->rc);
            if (node->namespace_buffer_field.bf_buffer->rc == 0) {
              kfree(node->namespace_buffer_field.bf_buffer->buffer);
              kfree(node->namespace_buffer_field.bf_buffer);
            }
          }

          acpi_unload_ns_node_in_parser(g_parser_ptr, node);
          //list_remove(invocation->resulting_ns_nodes, list_indexof(invocation, &node->namespace_processor));
        }

        acpi_state_pop_blk_stack(state);
        acpi_state_pop_ctx_stack(state);
        acpi_state_pop_stack(state);
        return 0;
      } else {
        parser_set_mode(g_parser_ptr, APM_EXEC);
        return parser_parse_acpi_state(state);
      }
      break;
    case ACPI_INTERP_STATE_BUFFER_STACKITEM: {
      int of_offset = state->operand_sp - stack->opstack_frame;
      if (of_offset == 1) {
        // TODO
        acpi_variable_t size_var = {0};

        acpi_operand_t* operand = acpi_operand_stack_get(state, stack->opstack_frame);

        acpi_get_obj_ref(state, operand, &size_var);
        acpi_state_pop_opstack(state);

        println(to_string(size_var.num));

        acpi_variable_t result = {0};
        acpi_var_create_buffer(&result, size_var.num);

        int size = block->program_counter_limit - block->program_counter;
        if (size < 0)
          kernel_panic("invalid buffer size");
        if (size > (int)result.buffer_p->size)
          kernel_panic("buffer size too big");

        memcpy(result.buffer_p->buffer, code_ptr + block->program_counter, size);

        if (stack->buffer.buffer_result_requested) {
          println("gave back result");
          acpi_operand_t* operand = acpi_state_push_opstack(state);
          operand->tag = ACPI_OPERAND_OBJECT;
          move_acpi_var(&operand->obj, &result);
        }

        destroy_acpi_var(&size_var);
        destroy_acpi_var(&result);

        acpi_state_pop_blk_stack(state);
        acpi_state_pop_stack(state);

        return 0;
      } else {
        println("parsing buffer stackitem");
        parser_set_mode(g_parser_ptr, APM_OBJECT);
        return parser_parse_acpi_state(state);
      }

      }
      break;
    case ACPI_INTERP_STATE_VARPACKAGE_STACKITEM:
    case ACPI_INTERP_STATE_PACKAGE_STACKITEM:
      ASSERT_MSG(acpi_operand_stack_get(state, stack->opstack_frame) != nullptr, "no operand while parsing stackitem!");
      acpi_operand_t* operand = acpi_operand_stack_get(state, stack->opstack_frame);

      if (stack->pkg.pkg_phase == 0) {
        if (stack->type == ACPI_INTERP_STATE_PACKAGE_STACKITEM) {
          parser_set_mode(g_parser_ptr, APM_IMMEDIATE_BYTE);
        } else {
          parser_set_mode(g_parser_ptr, APM_OBJECT);
        }
        int result = parser_parse_acpi_state(state);

        stack->pkg.pkg_phase++;
        return result;
      } else if (stack->pkg.pkg_phase == 1) {
        acpi_variable_t size_var = {0};
        // FIXME: is this enough?
        size_var = operand[1].obj;

        acpi_state_pop_opstack(state);

        if (acpi_var_create_package(&operand[0].obj, size_var.num) < 0) {
          destroy_acpi_var(&size_var);
          return -1;
        }
        
        stack->pkg.pkg_phase++;

        destroy_acpi_var(&size_var);
        return 0;
      }

      if (state->operand_sp == stack->opstack_frame + 2) {
        acpi_operand_t* package = &operand[0];
        ASSERT_MSG(package->tag == ACPI_OPERAND_OBJECT, "package operand is not an object");
        acpi_operand_t* initializer = &operand[1];
        ASSERT_MSG(initializer->tag == ACPI_OPERAND_OBJECT, "initializer operand is not an object");

        if (stack->pkg.pkg_idx >= (int)package->obj.package_p->pkg_size) {
          kernel_panic("package initializer too large");
        }

        println("pre store: ");
        println(to_string(state->operand_sp));
        
        acpi_store_package(&package->obj, &initializer->obj, stack->pkg.pkg_idx);
        stack->pkg.pkg_idx++;
        acpi_state_pop_opstack(state);

      }

      println("post or no store: ");
      println(to_string(state->operand_sp));

      ASSERT_MSG(state->operand_sp == stack->opstack_frame + 1, "ACPI: Something went wrong while initialising package");

      if (block->program_counter == block->program_counter_limit) {
        if (!stack->pkg.pkg_result_requested) {
          acpi_state_pop_opstack(state);
        }

        acpi_state_pop_blk_stack(state);
        acpi_state_pop_stack(state);
        return 0;
      } else {
        parser_set_mode(g_parser_ptr, APM_DATA);
        return parser_parse_acpi_state(state);
      }

      println("(parser_partial_execute_acpi_state): unimplemented interpreter state!");
      return -1;
    case ACPI_INTERP_STATE_NODE_STACKITEM: {
        int mode_idx = state->operand_sp - stack->opstack_frame;
        print("node_arg_mode: ");
        println(to_string(stack->node_opcode.node_arg_modes[mode_idx]));
        if (stack->node_opcode.node_arg_modes[mode_idx] == 0) {
          acpi_operand_t* op = &state->operand_stack_base[stack->opstack_frame];
          parser_parse_node(stack->node_opcode.node_opcode_code, state, op, ctx_handle);
          acpi_state_pop_opstack_n(state, mode_idx);
          acpi_state_pop_stack(state);
          return 0;
        } else {
          parser_set_mode(g_parser_ptr, stack->node_opcode.node_arg_modes[mode_idx]);
          return parser_parse_acpi_state(state);
        }
      }
      break;
    case ACPI_INTERP_STATE_OP_STACKITEM: {
        int mode_idx = state->operand_sp - stack->opstack_frame;
        if (stack->op_opcode.op_arg_modes[mode_idx] == 0) {
          if (acpi_operand_stack_ensure_capacity(state) < 0) {
            return -1;
          }

          acpi_variable_t result = {0};
          acpi_operand_t* op = &state->operand_stack_base[stack->opstack_frame];
          if (parser_parse_op(stack->op_opcode.op_opcode, state, op, &result) < 0) {
            return -1;
          }

          acpi_state_pop_opstack_n(state, mode_idx);

          if (stack->op_opcode.op_result_requested) {
            acpi_operand_t* op_result = acpi_state_push_opstack(state);
            op_result->tag = ACPI_OPERAND_OBJECT;
            move_acpi_var(&op_result->obj, &result);
          } else {
            destroy_acpi_var(&result);
          }
          acpi_state_pop_stack(state);
          return 0;
        } else {
          parser_set_mode(g_parser_ptr, stack->op_opcode.op_arg_modes[mode_idx]);
          return parser_parse_acpi_state(state);
        }
      }
      break;
    case ACPI_INTERP_STATE_INVOKE_STACKITEM:
      println("INVOKE_STACKITEM");
      return -1;
    case ACPI_INTERP_STATE_RETURN_STACKITEM:
      println("RETURN_STACKITEM");
      return -1;
    case ACPI_INTERP_STATE_LOOP_STACKITEM:
      println("LOOP_STACKITEM");
      return -1;
    case ACPI_INTERP_STATE_COND_STACKITEM:
      println("BANKFIELD_STACKITEM");
      return -1;
    case ACPI_INTERP_STATE_BANKFIELD_STACKITEM:
    default:
      println("(parser_partial_execute_acpi_state): unimplemented interpreter state!");
      return -1;
  }

  return -1;
}

static int handle_zero_op(acpi_state_t* state, int pc);
static int handle_integer_op(acpi_state_t* state, int pc, uintptr_t integer);
static int handle_timer_op(acpi_state_t* state, int pc);
static int handle_integer_prefix_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode, int pc);
static int handle_string_prefix_op(acpi_state_t* state, acpi_block_entry_t* block, uint8_t* code_ptr, int pc);
static int handle_buffer_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc);
static int handle_varpackage_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc);
static int handle_package_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc);
static int handle_opregion_op(acpi_state_t* state, int opcode, int pc);
static int handle_method_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc, acpi_ns_node_t* ctx_handle, acpi_aml_seg_t* segment, acpi_invocation_t* invocation);
static int handle_device_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc, acpi_ns_node_t* ctx_handle, acpi_aml_seg_t* segment, acpi_invocation_t* invocation);
static int handle_name_op(acpi_state_t* state, int pc, int opcode);
static int handle_mutex_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int pc, acpi_ns_node_t* ctx_handle, acpi_invocation_t* invocation);

static int handle_scope_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc, acpi_ns_node_t* ctx_handle, acpi_aml_seg_t* segment);

int parser_parse_acpi_state(acpi_state_t* state) {

  acpi_context_entry_t* context = acpi_context_stack_peek(state);
  acpi_block_entry_t* block = acpi_block_stack_peek(state);

  acpi_aml_seg_t* aml_segment = context->segm;
  uint8_t* code_ptr = context->code;

  acpi_invocation_t* invocation = context->invocation;
  acpi_ns_node_t* ctx_handle = context->ctx_handle;

  int pc = block->program_counter;

  print("program_counter: ");
  println(to_string(pc));

  int opcode_pc = pc;
  int limit = block->program_counter_limit;

  // ayo how thefuq did this come to be??
  if (pc >= block->program_counter_limit) {
    // TODO: wtf
    //kernel_panic("fuck");
    return -1;
  }

  // sizeof the header + offset into the table + the program_counter in the block
  size_t table_rel_pc = sizeof(acpi_sdt_header_t) + (code_ptr - aml_segment->aml_table->data) + pc;

  // sizeof the header + offset into the table + the program_counter_limit of the block
  size_t table_rel_pc_limit = sizeof(acpi_sdt_header_t) + (code_ptr - aml_segment->aml_table->data) + block->program_counter_limit;

  if (block->program_counter >= block->program_counter_limit) {
    kernel_panic("parser_parse_acpi_state: pc seems to exceed the pc_limit!");
  }
  switch (g_parser_ptr->m_mode) {
    case APM_IMMEDIATE_BYTE: {
      uint8_t val = 0;
      if (acpi_parse_u8(&val, code_ptr, &pc, limit) < 0) {
        return -1;
      }

      if (acpi_operand_stack_ensure_capacity(state) < 0) {
        return -1;
      }

      parser_advance_block_pc(state, pc);

      acpi_operand_t* res = acpi_state_push_opstack(state);
      res->tag = ACPI_OPERAND_OBJECT;
      res->obj.var_type = ACPI_INTEGER;
      res->obj.num = val;
      return 0;
    }
    case APM_IMMEDIATE_WORD: {
      uint16_t val = 0;
      if (acpi_parse_u16(&val, code_ptr, &pc, limit) < 0) {
        return -1;
      }

      if (acpi_operand_stack_ensure_capacity(state) < 0) {
        return -1;
      }

      parser_advance_block_pc(state, pc);

      acpi_operand_t* res = acpi_state_push_opstack(state);
      res->tag = ACPI_OPERAND_OBJECT;
      res->obj.var_type = ACPI_INTEGER;
      res->obj.num = val;
      return 0;
    }
    case APM_IMMEDIATE_DWORD: {
      uint32_t val = 0;
      if (acpi_parse_u32(&val, code_ptr, &pc, limit) < 0) {
        return -1;
      }

      if (acpi_operand_stack_ensure_capacity(state) < 0) {
        return -1;
      }

      parser_advance_block_pc(state, pc);

      acpi_operand_t* res = acpi_state_push_opstack(state);
      res->tag = ACPI_OPERAND_OBJECT;
      res->obj.var_type = ACPI_INTEGER;
      res->obj.num = val;
      return 0;
    }
    default:
      break;
  }

  // TODO implement names
  if (acpi_is_name(code_ptr[pc])) {
    acpi_aml_name_t aml_name = {0};
    if (acpi_parse_aml_name(&aml_name, code_ptr + pc) < 0) {
      return -1;
    }

    pc += aml_name.m_size;

    if (acpi_operand_stack_ensure_capacity(state) < 0 || acpi_stack_ensure_capacity(state) < 0) {
      return -1;
    }

    parser_advance_block_pc(state, pc);
    
    bool should_pass_result = parser_has_flags(g_parser_ptr, PARSER_MODE_FLAG_EXPECT_RESULT);

    if (g_parser_ptr->m_mode == APM_DATA) {
      // nothing yet
      acpi_operand_t* op = acpi_state_push_opstack(state);
      op->tag = ACPI_OPERAND_OBJECT;
      op->obj.var_type = ACPI_LAZY_HANDLE;
      op->obj.unresolved_aml.unresolved_context_handle = ctx_handle;
      op->obj.unresolved_aml.unres_aml_p = code_ptr + opcode_pc;
    } else if (!parser_has_flags(g_parser_ptr, PARSER_MODE_FLAG_RESOLVE_NAME) && should_pass_result) {
      // here we shouldn't resolve the name, but just 
      // yeet it to the opstack lol
      if (acpi_operand_stack_ensure_capacity(state) < 0) {
        return -1;
      }

      acpi_operand_t* op = acpi_state_push_opstack(state);
      op->tag = ACPI_UNRESOLVED_NAME;
      op->unresolved_aml.unresolved_context_handle = ctx_handle;
      op->unresolved_aml.unres_aml_p = code_ptr + opcode_pc;

    } else {
      acpi_ns_node_t* handle = acpi_resolve_node(ctx_handle, &aml_name);

      if (!handle) {
        // TODO: handle edge-case
        if (parser_has_flags(g_parser_ptr, PARSER_MODE_FLAG_ALLOW_UNRESOLVED)) {
          // TODO:
          kernel_panic("TODO: found unresolved handle while in PARSER_MODE_FLAG_ALLOW_UNRESOLVED");
        }
        
        println("failed to parse handle: ");
        println(acpi_aml_name_to_string(&aml_name));
        return -1;
      }

      if (handle->type == ACPI_NAMESPACE_METHOD && parser_has_flags(g_parser_ptr, PARSER_MODE_FLAG_PARSE_INVOCATION)) {
        acpi_stack_entry_t* stack_item = acpi_state_push_stack_entry(state);
        stack_item->type = ACPI_INTERP_STATE_INVOKE_STACKITEM;
        stack_item->opstack_frame = state->operand_sp;
        stack_item->invoke.ivk_argc = handle->aml_method_flags & METHOD_ARGC_MASK;
        stack_item->invoke.ivk_result_requested = should_pass_result;

        acpi_operand_t* op = acpi_state_push_opstack(state);
        op->tag = ACPI_RESOLVED_NAME;
        op->handle = handle;
      } else if (parser_has_flags(g_parser_ptr, PARSER_MODE_FLAG_PARSE_INVOCATION)) {
        acpi_variable_t var = {0};
        acpi_load_object(&var, handle);

        if (should_pass_result) {
          acpi_operand_t* op = acpi_state_push_opstack(state);
          op->tag = ACPI_OPERAND_OBJECT;
          move_acpi_var(&op->obj, &var);
        }
      } else {
        if (should_pass_result) {
          acpi_operand_t* op = acpi_state_push_opstack(state);
          op->tag = ACPI_RESOLVED_NAME;
          op->handle = handle;
        }
      }
    }
    return 0;
  }

  int opcode;

  if (code_ptr[pc] == EXTOP_PREFIX) {
    if (pc + 1 >= block->program_counter_limit) {
      kernel_panic("parser_parse_acpi_state: exceeded pc limit while trying to parse aml opcode!");
    }
    opcode = (EXTOP_PREFIX << 8) | code_ptr[pc + 1];
    pc += 2;
  } else {
    opcode = code_ptr[pc];
    pc++;
  }

  print("opcode: ");
  println(to_string(opcode));

  switch (opcode) {
    case NOP_OP:
      ASSERT_MSG(parser_advance_block_pc(state, pc) == ANIVA_SUCCESS, "Could not advande pc!");
    case ZERO_OP:
      return handle_zero_op(state, pc);
    case ONE_OP:
      handle_integer_op(state, pc, 0);
      break;
    case ONES_OP:
      return handle_integer_op(state, pc, ~((uintptr_t)0));
    case (EXTOP_PREFIX << 8) | REVISION_OP:
      // TODO: have a revision
      return handle_integer_op(state, pc, 0x69420);
    case (EXTOP_PREFIX << 8) | TIMER_OP:
      return handle_timer_op(state, pc);
    case BYTEPREFIX:
    case WORDPREFIX:
    case DWORDPREFIX:
    case QWORDPREFIX: 
      return handle_integer_prefix_op(state, code_ptr, limit, opcode, pc);
    case STRINGPREFIX:
      return handle_string_prefix_op(state, block, code_ptr, pc);
    case BUFFER_OP:
      return handle_buffer_op(state, code_ptr, limit, opcode_pc, pc);
    case VARPACKAGE_OP:
      return handle_varpackage_op(state, code_ptr, limit, opcode_pc, pc);
    case PACKAGE_OP:
      return handle_package_op(state, code_ptr, limit, opcode_pc, pc);
    case RETURN_OP:
    case WHILE_OP:
    case CONTINUE_OP:
    case BREAK_OP:
    case IF_OP:
    case ELSE_OP:
      println("some unimplemented controlflow operators");
      return -1;
    case SCOPE_OP:
      // TODO: first node seems to be a SCOPE_OP, so lets impl this one first
      return handle_scope_op(state, code_ptr, limit, opcode_pc, pc, ctx_handle, context->segm);
    case (EXTOP_PREFIX << 8) | DEVICE:
      return handle_device_op(state, code_ptr, limit, opcode_pc, pc, ctx_handle, context->segm, invocation);
      break;
    case (EXTOP_PREFIX << 8) | PROCESSOR:
      kernel_panic("proc");
      break;
    case (EXTOP_PREFIX << 8) | POWER_RES:
      kernel_panic("power res");
      break;
    case (EXTOP_PREFIX << 8) | THERMALZONE:
      break;
      kernel_panic("thermal");
      break;
    case METHOD_OP: 
      return handle_method_op(state, code_ptr, limit, opcode_pc, pc, ctx_handle, context->segm, invocation);
    case EXTERNAL_OP:
      kernel_panic("unimplemented opcode!");
    case NAME_OP:
      return handle_name_op(state, pc, opcode);
    case ALIAS_OP:
    case BITFIELD_OP:
    case BYTEFIELD_OP:
    case WORDFIELD_OP:
    case QWORDFIELD_OP:
    case (EXTOP_PREFIX << 8) | ARBFIELD_OP:
      kernel_panic("(parser_parse_acpi_state): unimplemented opcode!");
      break;
    case (EXTOP_PREFIX << 8) | MUTEX:
      return handle_mutex_op(state, code_ptr, limit, pc, ctx_handle, invocation);
      println("mutex");
      kernel_panic("(parser_parse_acpi_state): unimplemented opcode!");
      break;
    case (EXTOP_PREFIX << 8) | EVENT:
      println("some fields, mutex and event");
      kernel_panic("(parser_parse_acpi_state): unimplemented opcode!");
      break;
    case (EXTOP_PREFIX << 8) | OPREGION:
      return handle_opregion_op(state, opcode, pc);
    case (EXTOP_PREFIX << 8) | FIELD:
      println("field");
      size_t package_size;
      acpi_aml_name_t aml_name = {0};
      if (parse_acpi_var_int(&package_size, code_ptr, &pc, limit) < 0 || acpi_parse_aml_name(&aml_name, code_ptr + pc) < 0) {
        return -1;
      }

      pc += aml_name.m_size;

      int end_pc = opcode_pc + 2 + package_size;

      acpi_ns_node_t* reg_node = acpi_resolve_node(ctx_handle, &aml_name);

      if (!reg_node) {
        kernel_panic("could not resolve node");
      }

      uint8_t access_type = *(code_ptr + pc);
      pc++;

      uintptr_t current_offset;
      uintptr_t skip_bits;
      acpi_aml_name_t field_name = {0};

      while (pc < end_pc) {
        switch (*(code_ptr + pc)) {
          case 0:
            pc++;
            if (parse_acpi_var_int(&skip_bits, code_ptr, &pc, limit) < 0) {
              return -1;
            }
            current_offset += skip_bits;
            break;
          case 1:
            pc++;
            access_type = *(code_ptr + pc);
            pc += 2;
            break;
          case 2:
            // TODO
            return -1;
          default:
            if (acpi_parse_aml_name(&field_name, code_ptr + pc) < 0) {
              return -1;
            }
            pc += field_name.m_size;
            if (parse_acpi_var_int(&skip_bits, code_ptr, &pc, limit) < 0) {
              return -1;
            }
            acpi_ns_node_t* node = acpi_create_node();
            node->type = ACPI_NAMESPACE_FIELD;
            node->namespace_field.field_region_node = reg_node;
            node->namespace_field.field_flags = access_type;
            node->namespace_field.field_size_bits = skip_bits;
            node->namespace_field.field_offset_bits = current_offset;

            acpi_resolve_new_node(node, ctx_handle, &field_name);
            acpi_load_ns_node_in_parser(g_parser_ptr, node);

            if (invocation) {
              //list_append(invocation->resulting_ns_nodes, &node->)
              kernel_panic("FIELD: invocation");
            }

            current_offset += skip_bits;
        }
      }

      parser_advance_block_pc(state, pc);
      return 0;
    case (EXTOP_PREFIX << 8) | INDEXFIELD:
    case (EXTOP_PREFIX << 8) | BANKFIELD:
    case ARG0_OP:
    case ARG1_OP:
    case ARG2_OP:
    case ARG3_OP:
    case ARG4_OP:
    case ARG5_OP:
    case ARG6_OP:
    case LOCAL0_OP:
    case LOCAL1_OP:
    case LOCAL2_OP:
    case LOCAL3_OP:
    case LOCAL4_OP:
    case LOCAL5_OP:
    case LOCAL6_OP:
    case BREAKPOINT_OP:
    case TOBUFFER_OP:
    case TODECIMALSTRING_OP:
    case TOHEXSTRING_OP:
    case TOINTEGER_OP:
    case TOSTRING_OP:
    case MID_OP:
    case (EXTOP_PREFIX << 8) | FATAL_OP:
    case (EXTOP_PREFIX << 8) | DEBUG_OP:
    case STORE_OP:
    case COPYOBJECT_OP:
    case NOT_OP:
    case FINDSETLEFTBIT_OP:
    case FINDSETRIGHTBIT_OP:
    case CONCAT_OP:
    case ADD_OP:
    case SUBTRACT_OP:
    case MOD_OP:
    case MULTIPLY_OP:
    case AND_OP:
    case OR_OP:
    case XOR_OP:
    case SHR_OP:
    case SHL_OP:
    case NAND_OP:
    case NOR_OP:
    case DIVIDE_OP:
    case INCREMENT_OP:
    case DECREMENT_OP:
    case LNOT_OP:
    case LAND_OP:
    case LOR_OP:
    case LEQUAL_OP:
    case LLESS_OP:
    case LGREATER_OP:
    case INDEX_OP:
    case MATCH_OP:
    case CONCATRES_OP:
    case OBJECTTYPE_OP:
    case DEREF_OP:
    case SIZEOF_OP:
    case REFOF_OP:
    case NOTIFY_OP:
    case (EXTOP_PREFIX << 8) | CONDREF_OP:
    case (EXTOP_PREFIX << 8) | STALL_OP:
    case (EXTOP_PREFIX << 8) | SLEEP_OP:
    case (EXTOP_PREFIX << 8) | ACQUIRE_OP:
    case (EXTOP_PREFIX << 8) | WAIT_OP:
    case (EXTOP_PREFIX << 8) | SIGNAL_OP:
    case (EXTOP_PREFIX << 8) | RESET_OP:
    case (EXTOP_PREFIX << 8) | FROM_BCD_OP:
    case (EXTOP_PREFIX << 8) | TO_BCD_OP:
    default:
      println("thats a lot of stuff");
      println("(parser_parse_acpi_state): unimplemented opcode!");
      return -1;
  }

  return 0;
}

static int handle_zero_op(acpi_state_t* state, int pc) {
  if (acpi_operand_stack_ensure_capacity(state) < 0) {
    return -1;
  }
  parser_advance_block_pc(state, pc);

  switch (g_parser_ptr->m_mode) {
    case APM_OBJECT:
    case APM_DATA :
      {
      acpi_operand_t* op = acpi_state_push_opstack(state);
      op->tag = ACPI_OPERAND_OBJECT;
      op->obj.var_type = ACPI_TYPE_INTEGER;
      op->obj.num = 0;
      }
      break;
    case APM_OPTIONAL_REFERENCE:
    case APM_REFERENCE:
      {
      acpi_operand_t* op = acpi_state_push_opstack(state);
      op->tag = ACPI_NULL_NAME;
      }
      break;
    default:
      break;
  }
  return 0;
}

static int handle_integer_op(acpi_state_t* state, int pc, uintptr_t integer) {
  if (acpi_operand_stack_ensure_capacity(state) < 0) {
    return -1;
  }
  parser_advance_block_pc(state, pc);

  switch (g_parser_ptr->m_mode) {
    case APM_OBJECT:
    case APM_DATA :
      {
      acpi_operand_t* op = acpi_state_push_opstack(state);
      op->tag = ACPI_OPERAND_OBJECT;
      op->obj.var_type = ACPI_TYPE_INTEGER;
      op->obj.num = integer;
      }
    default:
      break;
  }
  return 0;
}

static int handle_timer_op(acpi_state_t* state, int pc) {
  if (acpi_operand_stack_ensure_capacity(state) < 0) {
    return -1;
  }
  parser_advance_block_pc(state, pc);

  switch (g_parser_ptr->m_mode) {
    case APM_OBJECT:
    case APM_DATA :
      {
        {
          kernel_panic("TODO: implement timer functionality");
        }
        acpi_operand_t* op = acpi_state_push_opstack(state);
        op->tag = ACPI_OPERAND_OBJECT;
        op->obj.var_type = ACPI_TYPE_INTEGER;
        // TODO: have a revision of the interpreter
        op->obj.num = 0;
      }
    default:
      break;
  }
  return 0;
}

static int handle_integer_prefix_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode, int pc) {
  uintptr_t value = 0;
  if (opcode == BYTEPREFIX) {
    uint8_t tmp;
    if (acpi_parse_u8(&tmp, code_ptr, &pc, limit) < 0) {
      // die
      return -1;
    }
    value = tmp;
  } else if (opcode == WORDPREFIX) {
    uint16_t tmp;
    if (acpi_parse_u16(&tmp, code_ptr, &pc, limit) < 0) {
      // die
      return -1;
    }
    value = tmp;
  } else if (opcode == DWORDPREFIX) {
    uint32_t tmp;
    if (acpi_parse_u32(&tmp, code_ptr, &pc, limit) < 0) {
      // die
      return -1;
    }
    value = tmp;
  } else if (opcode == QWORDPREFIX) {
    uint64_t tmp;
    if (acpi_parse_u64(&tmp, code_ptr, &pc, limit) < 0) {
      // die
      return -1;
    }
    value = tmp;
  }

  return handle_integer_op(state, pc, value);
}

static int handle_string_prefix_op(acpi_state_t* state, acpi_block_entry_t* block, uint8_t* code_ptr, int pc) {
  uintptr_t length = 0;
  while (pc + length < (uintptr_t)block->program_counter_limit && code_ptr[pc + length] != 0) {
    length++;
  }
  int pc_backup = pc;
  pc += length + 1;

  if (acpi_operand_stack_ensure_capacity(state) < 0) {
    return -1;
  }
  parser_advance_block_pc(state, pc);

  if (g_parser_ptr->m_mode == APM_DATA || g_parser_ptr->m_mode == APM_OBJECT) {
    acpi_operand_t* op = acpi_state_push_opstack(state);
    op->tag = ACPI_OPERAND_OBJECT;
    if (acpi_var_create_str(&op->obj, length) < 0) {
      kernel_panic("handle_string_prefix_op: failed to create acpi string var");
    }
    memcpy(&op->obj.str_p->str, code_ptr + pc_backup, length);
  }
  return 0;
}

static int handle_buffer_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc) {
  size_t buff_size;

  if (parse_acpi_var_int(&buff_size, code_ptr, &pc, limit) < 0) {
    return -1;
  }
  int pc_backup = pc;
  const int pc_bump = opcode_pc + 1 + buff_size;
  pc = pc_bump;

  if (acpi_block_stack_ensure_capacity(state) < 0 || acpi_stack_ensure_capacity(state) < 0) {
    return -1;
  }

  parser_advance_block_pc(state, pc);

  acpi_block_entry_t* blk_entry = acpi_state_push_block_entry(state);
  blk_entry->program_counter = pc_backup;
  blk_entry->program_counter_limit = pc_bump;

  acpi_stack_entry_t* stack_entry = acpi_state_push_stack_entry(state);
  stack_entry->type = ACPI_INTERP_STATE_BUFFER_STACKITEM;
  stack_entry->opstack_frame = state->operand_sp;
  stack_entry->buffer.buffer_result_requested = parser_has_flags(g_parser_ptr, PARSER_MODE_FLAG_EXPECT_RESULT);
  return 0;
}

static int handle_varpackage_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc) {

  size_t pkg_size;

  if (parse_acpi_var_int(&pkg_size, code_ptr, &pc, limit) < 0) {
    return -1;
  }
  int pc_backup = pc;
  const int pc_bump = opcode_pc + 1 + pkg_size;
  pc = pc_bump;

  if (acpi_operand_stack_ensure_capacity(state) < 0 || acpi_block_stack_ensure_capacity(state) < 0 || acpi_stack_ensure_capacity(state) < 0) {
    return -1;
  }

  parser_advance_block_pc(state, pc);

  acpi_block_entry_t* blk_entry = acpi_state_push_block_entry(state);
  blk_entry->program_counter = pc_backup;
  blk_entry->program_counter_limit = pc_bump;

  acpi_stack_entry_t* stack_entry = acpi_state_push_stack_entry(state);
  stack_entry->type = ACPI_INTERP_STATE_VARPACKAGE_STACKITEM;
  stack_entry->opstack_frame = state->operand_sp;
  stack_entry->pkg.pkg_idx = 0;
  stack_entry->pkg.pkg_phase = 0;
  stack_entry->pkg.pkg_result_requested = false;

  acpi_operand_t* op_entry = acpi_state_push_opstack(state);
  op_entry->tag = ACPI_OPERAND_OBJECT;
  return 0;
}

static int handle_package_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc) {

  size_t pkg_size;

  if (parse_acpi_var_int(&pkg_size, code_ptr, &pc, limit) < 0) {
    return -1;
  }
  int pc_backup = pc;
  const int pc_bump = opcode_pc + 1 + pkg_size;
  pc = pc_bump;

  if (acpi_operand_stack_ensure_capacity(state) < 0 || acpi_block_stack_ensure_capacity(state) < 0 || acpi_stack_ensure_capacity(state) < 0) {
    return -1;
  }

  parser_advance_block_pc(state, pc);

  acpi_block_entry_t* blk_entry = acpi_state_push_block_entry(state);
  blk_entry->program_counter = pc_backup;
  blk_entry->program_counter_limit = pc_bump;

  acpi_stack_entry_t* stack_entry = acpi_state_push_stack_entry(state);
  stack_entry->type = ACPI_INTERP_STATE_PACKAGE_STACKITEM;
  stack_entry->opstack_frame = state->operand_sp;
  stack_entry->pkg.pkg_idx = 0;
  stack_entry->pkg.pkg_phase = 0;
  stack_entry->pkg.pkg_result_requested = false;

  acpi_operand_t* op_entry = acpi_state_push_opstack(state);
  op_entry->tag = ACPI_OPERAND_OBJECT;
  return 0;
}


static int handle_scope_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc, acpi_ns_node_t* ctx_handle, acpi_aml_seg_t* segment) {
  size_t encoded_size;
  acpi_aml_name_t aml_name = {0};

  if (parse_acpi_var_int(&encoded_size, code_ptr, &pc, limit) < 0) {
    return -1;
  }

  acpi_parse_aml_name(&aml_name, code_ptr + pc);
  
  pc += aml_name.m_size;

  int data_pc = pc;
  const int pc_bump = opcode_pc + 1 + encoded_size;
  pc = pc_bump;

  if (acpi_context_stack_ensure_capacity(state) < 0 || acpi_block_stack_ensure_capacity(state) < 0 || acpi_stack_ensure_capacity(state) < 0) {
    return -1;
  }

  parser_advance_block_pc(state, pc);

  acpi_ns_node_t* scoped_ctx_handle = acpi_resolve_node(ctx_handle, &aml_name);

  if (!scoped_ctx_handle) {
    kernel_panic("handle_scope_op: failed to resolve node reference in Scope()");
  }

  acpi_context_entry_t* ctx_entry = acpi_state_push_context_entry(state);
  ctx_entry->segm = segment;
  ctx_entry->code = code_ptr;
  ctx_entry->ctx_handle = scoped_ctx_handle;

  acpi_block_entry_t* block_entry = acpi_state_push_block_entry(state);
  block_entry->program_counter = data_pc;
  block_entry->program_counter_limit = pc_bump;

  acpi_stack_entry_t* stack_entry = acpi_state_push_stack_entry(state);
  stack_entry->type = ACPI_INTERP_STATE_POPULATE_STACKITEM;

  return 0;
}

static int handle_opregion_op(acpi_state_t* state, int opcode, int pc) {
  if (acpi_stack_ensure_capacity(state) < 0) {
    return -1;
  }

  parser_advance_block_pc(state, pc);

  acpi_stack_entry_t* entry = acpi_state_push_stack_entry(state);
  entry->type = ACPI_INTERP_STATE_NODE_STACKITEM;
  entry->node_opcode.node_opcode_code = opcode;
  entry->opstack_frame = state->operand_sp;
  entry->node_opcode.node_arg_modes[0] = APM_UNRESOLVED;
  entry->node_opcode.node_arg_modes[1] = APM_IMMEDIATE_BYTE;
  entry->node_opcode.node_arg_modes[2] = APM_OBJECT;
  entry->node_opcode.node_arg_modes[3] = APM_OBJECT;
  entry->node_opcode.node_arg_modes[4] = 0;
  return 0;
}

static int handle_method_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc, acpi_ns_node_t* ctx_handle, acpi_aml_seg_t* segment, acpi_invocation_t* invocation) {

  size_t encoded_size;
  uint8_t flags;
  acpi_aml_name_t name = {0};

  if (parse_acpi_var_int(&encoded_size, code_ptr, &pc, limit) < 0) {
    return -1;
  }

  if (acpi_parse_aml_name(&name, code_ptr + pc) < 0) {
    return -1;
  }
  pc += name.m_size;

  if (acpi_parse_u8(&flags, code_ptr, &pc, limit) < 0) {
    return -1;
  }

  int nested_pc = pc;
  pc = opcode_pc + 1 + encoded_size;

  parser_advance_block_pc(state, pc);

  acpi_ns_node_t* node = acpi_create_node();

  node->type = ACPI_NAMESPACE_METHOD;

  acpi_resolve_new_node(node, ctx_handle, &name);

  node->aml_method_flags = flags;
  node->segment = segment;
  node->ptr = code_ptr + nested_pc;
  node->size = pc - nested_pc;

  acpi_load_ns_node_in_parser(g_parser_ptr, node);


  if (invocation) {
    kernel_panic("TODO: implement invocation (handle_method_op)");
  }

  return 0;
}

static int handle_device_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int opcode_pc, int pc, acpi_ns_node_t* ctx_handle, acpi_aml_seg_t* segment, acpi_invocation_t* invocation) {
  int nested_pc;
  int pc_bump;
  size_t encoded_size;
  acpi_aml_name_t name = {0};
  acpi_ns_node_t* node;

  acpi_context_entry_t* context;
  acpi_block_entry_t* block;
  acpi_stack_entry_t* stack;
  
  if (parse_acpi_var_int(&encoded_size, code_ptr, &pc, limit) < 0) {
    return -1;
  }

  if (acpi_parse_aml_name(&name, code_ptr + pc) < 0) {
    return -1;
  }

  nested_pc = pc;
  pc_bump = encoded_size + 2 + opcode_pc;
  pc = pc_bump;

  if (acpi_stack_ensure_capacity(state) < 0 || acpi_context_stack_ensure_capacity(state) < 0 || acpi_block_stack_ensure_capacity(state) < 0) {
    return -1;
  } 

  parser_advance_block_pc(state, pc);

  node = acpi_create_node();
  node->type = ACPI_NAMESPACE_DEVICE;

  acpi_resolve_new_node(node, ctx_handle, &name);
  acpi_load_ns_node_in_parser(g_parser_ptr, node);

  if (invocation) {
    kernel_panic("TODO implement invocation");
  }

  context = acpi_state_push_context_entry(state);
  context->segm = segment;
  context->ctx_handle = ctx_handle;
  context->code = code_ptr;

  block = acpi_state_push_block_entry(state);
  block->program_counter = nested_pc;
  block->program_counter_limit = pc_bump;

  stack = acpi_state_push_stack_entry(state);
  stack->type = ACPI_INTERP_STATE_POPULATE_STACKITEM;

  println("device handled");
  return 0;
}

static int handle_name_op(acpi_state_t* state, int pc, int opcode) {
  if (acpi_stack_ensure_capacity(state) < 0) {
    return -1;
  }

  parser_advance_block_pc(state, pc);

  acpi_stack_entry_t* stack = acpi_state_push_stack_entry(state);
  stack->type = ACPI_INTERP_STATE_NODE_STACKITEM;
  stack->node_opcode.node_opcode_code = opcode;
  stack->opstack_frame = state->operand_sp;
  stack->node_opcode.node_arg_modes[0] = APM_UNRESOLVED;
  stack->node_opcode.node_arg_modes[1] = APM_OBJECT;
  stack->node_opcode.node_arg_modes[2] = 0;
  return 0;
}

static int handle_mutex_op(acpi_state_t* state, uint8_t* code_ptr, int limit, int pc, acpi_ns_node_t* ctx_handle, acpi_invocation_t* invocation) {

  println("mutex");
  acpi_aml_name_t name = {0};

  if (acpi_parse_aml_name(&name, code_ptr + pc) < 0) {
    return -1;
  }

  // add size plus trailing byte
  pc += name.m_size + 1;

  parser_advance_block_pc(state, pc);

  acpi_ns_node_t* node = acpi_create_node();
  node->type = ACPI_NAMESPACE_MUTEX;

  acpi_resolve_new_node(node, ctx_handle, &name);

  acpi_load_ns_node_in_parser(g_parser_ptr, node);

  if (invocation) {
    kernel_panic("TODO: implement invocation");
  }
  return 0;
}
/*
 * ACPI opcode parsing functions
 */

static size_t parse_acpi_var_int(size_t* out, uint8_t *code_ptr, int* pc, int limit) {
  if (*pc + 1 > limit)
      return -1;
  uint8_t sz = (code_ptr[*pc] >> 6) & 3;
  if (!sz) {
    *out = (size_t)(code_ptr[*pc] & 0x3F);
    (*pc)++;
    return 0;
  } else if (sz == 1) {
    if (*pc + 2 > limit)
      return -1;
    *out = (size_t)(code_ptr[*pc] & 0x0F) | (size_t)(code_ptr[*pc + 1] << 4);
    *pc += 2;
    return 0;
  } else if (sz == 2) {
    if (*pc + 3 > limit)
      return -1;
    *out = (size_t)(code_ptr[*pc] & 0x0F) | (size_t)(code_ptr[*pc + 1] << 4)
           | (size_t)(code_ptr[*pc + 2] << 12);
    *pc += 3;
    return 0;
  } else {
    if (*pc + 4 > limit || sz != 3)
      return -1;
    *out = (size_t)(code_ptr[*pc] & 0x0F) | (size_t)(code_ptr[*pc + 1] << 4)
           | (size_t)(code_ptr[*pc + 2] << 12) | (size_t)(code_ptr[*pc + 3] << 20);
    *pc += 4;
    return 0;
  }
}

static ALWAYS_INLINE int acpi_parse_u8(uint8_t* out, uint8_t* code_ptr, int* pc, int limit) {
  if (*pc + 1 > limit) {
    return -1;
  }

  *out = code_ptr[*pc];
  (*pc)++;
  return 0;
}

static ALWAYS_INLINE int acpi_parse_u16(uint16_t* out, uint8_t* code_ptr, int* pc, int limit) {
  if (*pc + 2 > limit) {
    return -1;
  }

  *out = ((uint16_t)code_ptr[*pc]) | (((uint16_t)code_ptr[*pc + 1]) << 8);
  *pc += 2;
  return 0;
}

static ALWAYS_INLINE int acpi_parse_u32(uint32_t* out, uint8_t* code_ptr, int* pc, int limit) {
  if (*pc + 4 > limit) {
    return -1;
  }

  *out = ((uint32_t)code_ptr[*pc])              | (((uint32_t)code_ptr[*pc + 1]) << 8)
      |  (((uint32_t) code_ptr[*pc + 2]) << 16) | (((uint32_t) code_ptr[*pc + 3]) << 24);
  *pc += 4;
  return 0;
}

static ALWAYS_INLINE int acpi_parse_u64(uint64_t* out, uint8_t* code_ptr, int* pc, int limit) {
  if (*pc + 8 > limit) {
    return -1;
  }

  *out = ((uint64_t)code_ptr[*pc])              | (((uint64_t)code_ptr[*pc + 1]) << 8)
      |  (((uint64_t) code_ptr[*pc + 2]) << 16) | (((uint64_t) code_ptr[*pc + 3]) << 24)
      |  (((uint64_t) code_ptr[*pc + 4]) << 32) | (((uint64_t) code_ptr[*pc + 5]) << 40)
      |  (((uint64_t) code_ptr[*pc + 6]) << 48) | (((uint64_t) code_ptr[*pc + 7]) << 56);
  *pc += 8;
  return 0;
}
