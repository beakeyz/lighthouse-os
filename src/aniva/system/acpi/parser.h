#ifndef __ANIVA_ACPI_PARSER__ 
#define __ANIVA_ACPI_PARSER__
#include <libk/stddef.h>
#include "dev/debug/serial.h"
#include "libk/linkedlist.h"
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

#define PARSER_MODE_FLAG_EXPECT_RESULT 1
#define PARSER_MODE_FLAG_RESOLVE_NAME 2
#define PARSER_MODE_FLAG_ALLOW_UNRESOLVED 4
#define PARSER_MODE_FLAG_PARSE_INVOCATION 8

enum acpi_parser_rsdp_discovery_method {
  MULTIBOOT_OLD,
  MULTIBOOT_NEW,
  BIOS_POKE,
  RECLAIM_POKE,
  NONE,
};

// TODO: fill structure
// TODO: should the acpi parser have its own heap?
typedef struct acpi_parser {
  acpi_rsdp_t* m_rsdp;
  acpi_xsdp_t* m_xsdp;
  bool m_is_xsdp;

  list_t* m_tables;

  acpi_fadt_t *m_fadt;
  uintptr_t m_multiboot_addr;

  // NOTE: this is temporary debug, remove ASAP
  char* m_last_error_message;
  enum acpi_parser_rsdp_discovery_method m_rsdp_discovery_method;

  int m_acpi_rev;

  acpi_state_t m_state;

  // acpi aml namespace nodes
  acpi_ns_node_t* m_ns_root_node;
  list_t* m_namespace_nodes;

  spinlock_t* m_mode_lock;
  uint8_t m_mode_flags;
  enum acpi_parser_mode m_mode;

} acpi_parser_t;

void init_acpi_parser(acpi_parser_t* parser, uintptr_t multiboot_addr);

void init_acpi_parser_aml(acpi_parser_t* parser);

void parser_init_tables(acpi_parser_t* parser);

// find rsdt and (if available) xsdt
void* find_rsdp(acpi_parser_t* parser);

// me want cool table
void* find_table_idx(acpi_parser_t *parser, const char* sig, size_t index);
void* find_table(acpi_parser_t *parser, const char* sig);

// just for funzies
void print_tables(void* rsdp_addr);

const char* parser_get_acpi_tables(acpi_parser_t* parser);

int parser_prepare_acpi_state(acpi_state_t* state, acpi_aml_seg_t* segment, acpi_ns_node_t* parent_node);

int parser_execute_acpi_state(acpi_state_t* state);

int parser_partial_execute_acpi_state(acpi_state_t* state);

int parser_evaluate_acpi_node(acpi_ns_node_t* node);

int parser_parse_acpi_state(acpi_state_t* state);

static ALWAYS_INLINE ANIVA_STATUS parser_advance_block_pc(acpi_state_t* state, int pc) {
  acpi_block_entry_t* entry = acpi_block_stack_peek(state);
  println(to_string(entry->program_counter_limit));
  if (entry) {
    entry->program_counter = pc;
    return ANIVA_SUCCESS;
  }
  return ANIVA_FAIL;
}

static ALWAYS_INLINE bool parser_is_mode(acpi_parser_t* parser, enum acpi_parser_mode mode) {
  return (parser->m_mode == mode);
}

static ALWAYS_INLINE void parser_set_flags(acpi_parser_t* parser, uint8_t flags) {
  parser->m_mode_flags = flags;
}

static ALWAYS_INLINE void parser_set_flag(acpi_parser_t* parser, uint8_t flag) {
  parser->m_mode_flags |= flag;
}

static ALWAYS_INLINE void parser_clear_flag(acpi_parser_t* parser, uint8_t flag) {
  parser->m_mode_flags &= ~flag;
}

static ALWAYS_INLINE bool parser_has_flags(acpi_parser_t* parser, uint8_t flags) {
  return (parser->m_mode_flags & flags);
}

// FIXME: is this locking redundant?
static ALWAYS_INLINE void parser_set_mode(acpi_parser_t* parser, enum acpi_parser_mode mode) {
  //spinlock_lock(parser->m_mode_lock);

  parser->m_mode = mode;

  switch (mode) {
    case APM_OBJECT:
      parser_set_flags(parser, PARSER_MODE_FLAG_EXPECT_RESULT | PARSER_MODE_FLAG_RESOLVE_NAME | PARSER_MODE_FLAG_PARSE_INVOCATION);
      break;
    case APM_EXEC:
      parser_set_flags(parser, PARSER_MODE_FLAG_RESOLVE_NAME | PARSER_MODE_FLAG_PARSE_INVOCATION);
      break;
    case APM_REFERENCE:
      parser_set_flags(parser, PARSER_MODE_FLAG_EXPECT_RESULT | PARSER_MODE_FLAG_RESOLVE_NAME);
      break;
    case APM_OPTIONAL_REFERENCE:
      parser_set_flags(parser, PARSER_MODE_FLAG_EXPECT_RESULT | PARSER_MODE_FLAG_RESOLVE_NAME | PARSER_MODE_FLAG_ALLOW_UNRESOLVED);
      break;
    case APM_DATA:
    case APM_UNRESOLVED:
    case APM_IMMEDIATE_BYTE:
    case APM_IMMEDIATE_WORD:
    case APM_IMMEDIATE_DWORD:
      parser_set_flags(parser, PARSER_MODE_FLAG_EXPECT_RESULT);
      break;
  }

  //spinlock_unlock(parser->m_mode_lock);
}

extern acpi_parser_t *g_parser_ptr;
#endif // !
