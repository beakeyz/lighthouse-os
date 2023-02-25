#ifndef __ANIVA_ACPI_PARSER__ 
#define __ANIVA_ACPI_PARSER__
#include <libk/stddef.h>
#include "structures.h"
#include "acpi_obj.h"
#include "namespace.h"
#include "sync/spinlock.h"

enum acpi_parser_mode {
  APM_DATA = 1,
  APM_OBJECT,
  APM_EXEC,
  APM_UNRESOLVED,
  APM_REFERENCE,
  APM_OPTIONAL_REFERENCE,
  APM_IMMEDIATE_BYTE,
  APM_IMMEDIATE_WORD,
  APM_IMMEDIATE_DWORD,
};

// TODO: fill structure
// TODO: should the acpi parser have its own heap?
typedef struct acpi_parser {
  acpi_rsdp_t* m_rsdp;
  acpi_fadt_t *m_fadt;
  bool m_is_xsdp;

  int m_acpi_rev;

  acpi_state_t m_state;

  // acpi aml namespace nodes
  acpi_ns_node_t* m_ns_root_node;
  acpi_ns_node_t** m_ns_nodes;

  size_t m_ns_size;
  size_t m_ns_max_size;

  spinlock_t* m_mode_lock;
  enum acpi_parser_mode m_mode;

} acpi_parser_t;

void init_acpi_parser(acpi_parser_t* parser);

// find rsdt and (if available) xsdt
void* find_rsdp();

// me want cool table
void* find_table_idx(acpi_parser_t *parser, const char* sig, size_t index);
void* find_table(acpi_parser_t *parser, const char* sig);

// just for funzies
void print_tables(void* rsdp_addr);

int parser_prepare_acpi_state(acpi_state_t* state, acpi_aml_seg_t* segment, acpi_ns_node_t* parent_node);

int parser_execute_acpi_state(acpi_state_t* state);

int parser_partial_execute_acpi_state(acpi_state_t* state);

int parser_evaluate_acpi_node(acpi_ns_node_t* node);

int parser_parse_acpi_state(acpi_state_t* state);

static ALWAYS_INLINE ANIVA_STATUS parser_advance_block_pc(acpi_state_t* state, int pc) {
  acpi_block_entry_t* entry = acpi_block_stack_peek(state);
  if (entry) {
    entry->program_counter = pc;
    return ANIVA_SUCCESS;
  }
  return ANIVA_FAIL;
}

// FIXME: is this locking redundant?
static ALWAYS_INLINE void parser_set_mode(acpi_parser_t* parser, enum acpi_parser_mode mode) {
  lock_spinlock(parser->m_mode_lock);

  parser->m_mode = mode;

  unlock_spinlock(parser->m_mode_lock);
}

static ALWAYS_INLINE bool parser_is_mode(acpi_parser_t* parser, enum acpi_parser_mode mode) {
  return (parser->m_mode == mode);
}

extern acpi_parser_t *g_parser_ptr;
#endif // !
