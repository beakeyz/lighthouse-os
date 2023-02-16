#ifndef __ANIVA_ACPI_NAMESPACE__
#define __ANIVA_ACPI_NAMESPACE__
#include <libk/stddef.h>
#include "structures.h"
#include "acpi_obj.h"

struct acpi_aml_seg;
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
  int (*aml_method_override)(acpi_variable_t *args, acpi_variable_t *out);

} acpi_ns_node_t;

typedef struct acpi_aml_seg {
  acpi_aml_table_t *aml_table;
  size_t idx;
} acpi_aml_seg_t;

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
acpi_ns_node_t *acpi_get_root();
char* acpi_get_path_node(acpi_ns_node_t* node);

acpi_ns_node_t *acpi_get_node_by_path(acpi_ns_node_t* node, const char* path);
acpi_ns_node_t *acpi_enum_nodes(char*, size_t);

acpi_aml_seg_t *acpi_load_segment(void* table_ptr, int idx);

#endif //__ANIVA_ACPI_NAMESPACE__
