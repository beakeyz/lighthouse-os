#ifndef __ANIVA_ACPI_NAMESPACE__
#define __ANIVA_ACPI_NAMESPACE__
#include <libk/stddef.h>
#include "libk/linkedlist.h"
#include "structures.h"
#include "acpi_obj.h"
#include "opcodes.h"
#include <libk/tuple.h>

struct acpi_aml_seg;
struct acpi_parser;
enum acpi_ns_node_type;

typedef struct acpi_ns_node {
  uint32_t name;
  uint32_t type;

  void* ptr;
  size_t size;

  struct acpi_aml_seg* segment;
  struct acpi_ns_node* parent;

  acpi_variable_t object;

  int (*aml_notify_override)(struct acpi_ns_node*, int, void*);

  uint8_t aml_method_flags;
  int (*aml_method_override)(acpi_variable_t *args, acpi_variable_t *out);

  union {
    struct acpi_ns_node *namespace_alias_target;

    struct {
      struct acpi_ns_node* field_region_node;
      uint64_t field_offset_bits;
      size_t field_size_bits;
      uint8_t field_flags;

      union {
        struct {
          struct acpi_ns_node* field_bank_node;
          uint64_t field_bankfield_value;
        } field_bankfield;
        struct {
          struct acpi_ns_node* index_node;
          struct acpi_ns_node* data_node;
        } field_indexfield;
      };
    } namespace_field;

    struct {
      struct acpi_buffer *bf_buffer;
      uintptr_t bf_offset_bits;
      uintptr_t bf_size_bits;
    } namespace_buffer_field;

    struct {
      uint8_t cpu_id;
      uint32_t processor_block_address;
      uint8_t processor_block_length;
    } namespace_processor;
    struct {
      uint8_t op_addr_space;
      uint8_t op_base;
      uint8_t op_length;
      // op region override
      void* op_user_ptr;
    } namespace_opregion;
    struct {
      // TODO:
    } namespace_mutex;
    struct {
      // TODO:
    } namespace_event;
  };

  size_t m_children_count;
  size_t m_max_children_count;
  list_t* m_children_list;

} acpi_ns_node_t;

typedef struct acpi_aml_seg {
  acpi_aml_table_t *aml_table;
  size_t idx;
} acpi_aml_seg_t;

typedef struct acpi_aml_name {
  // TODO:
  bool m_absolute;
  bool m_should_search_parent_scopes;
  uint32_t m_scope_height;
  size_t m_size;

  const uint8_t* m_itterator_p;
  const uint8_t* m_start_p;
  const uint8_t* m_end_p;
} acpi_aml_name_t;

#define ACPI_NAMESPACE_ROOT 1
#define ACPI_NAMESPACE_NAME 2
#define ACPI_NAMESPACE_ALIAS 3
#define ACPI_NAMESPACE_FIELD 4
#define ACPI_NAMESPACE_METHOD 5
#define ACPI_NAMESPACE_DEVICE 6
#define ACPI_NAMESPACE_INDEXFIELD 7
#define ACPI_NAMESPACE_MUTEX 8
#define ACPI_NAMESPACE_PROCESSOR 9
#define ACPI_NAMESPACE_BUFFER_FIELD 10
#define ACPI_NAMESPACE_THERMALZONE 11
#define ACPI_NAMESPACE_EVENT 12
#define ACPI_NAMESPACE_POWERRESOURCE 13
#define ACPI_NAMESPACE_BANKFIELD 14
#define ACPI_NAMESPACE_OPREGION 15

typedef enum acpi_ns_node_type {
  ACPI_NODETYPE_NULL,
  ACPI_NODETYPE_ROOT,
  ACPI_NODETYPE_EVALUATABLE,
  ACPI_NODETYPE_DEVICE,
  ACPI_NODETYPE_MUTEX,
  ACPI_NODETYPE_PROCESSOR,
  ACPI_NODETYPE_THERMALZONE,
  ACPI_NODETYPE_EVENT,
  ACPI_NODETYPE_POWERRESOURCE,
  ACPI_NODETYPE_OPREGION,
} acpi_ns_node_type_t;

acpi_ns_node_t *acpi_create_root();
acpi_ns_node_t *acpi_get_root(struct acpi_parser* parser);
char* acpi_get_path_node(acpi_ns_node_t* node);

acpi_ns_node_t* acpi_create_node();

acpi_ns_node_t *acpi_get_node_by_path(acpi_ns_node_t* node, const char* path);
acpi_ns_node_t *acpi_get_node_child(acpi_ns_node_t* handle, const char* name);
acpi_ns_node_t *acpi_enum_nodes(char*, size_t);

acpi_aml_seg_t *acpi_load_segment(void* table_ptr, int idx);

void acpi_load_ns_node_in_parser(struct acpi_parser* parser, acpi_ns_node_t* node);
void acpi_unload_ns_node_in_parser(struct acpi_parser* parser, acpi_ns_node_t* node);

/*
 * AML names
 */

int acpi_parse_aml_name(acpi_aml_name_t* name, const void* data);

char* acpi_aml_name_to_string(acpi_aml_name_t* name);
char* acpi_get_absolute_node_path(acpi_ns_node_t* node);

acpi_ns_node_t* acpi_resolve_node(acpi_ns_node_t* handle, acpi_aml_name_t* aml_name);
void acpi_resolve_new_node(acpi_ns_node_t *node, acpi_ns_node_t* handle, acpi_aml_name_t* aml_name);

static ALWAYS_INLINE bool acpi_is_name(char c) {
  return ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || c == '_' || c == ROOT_CHAR || c == PARENT_CHAR || c == MULTI_PREFIX || c == DUAL_PREFIX);
}

static ALWAYS_INLINE void acpi_aml_name_next_segment(acpi_aml_name_t* name, char* buffer) {
  for (uint32_t i = 0; i < 4; i++) {
    buffer[i] = name->m_itterator_p[i];
  }
  // next segment
  name->m_itterator_p += 4;
}

/*
 * acpi_ns_node children management
 */

bool ns_node_has_child(acpi_ns_node_t* handle, acpi_ns_node_t* node);
void ns_node_insert_child(acpi_ns_node_t* handle, acpi_ns_node_t* child);
void ns_node_remove_child(acpi_ns_node_t* handle, acpi_ns_node_t* child);
acpi_ns_node_t* ns_node_get_child_by_name(acpi_ns_node_t* handle, const char* name);

#endif //__ANIVA_ACPI_NAMESPACE__
