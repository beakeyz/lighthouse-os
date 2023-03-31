#ifndef __ANIVA_ACPI_NAMESPACE__
#define __ANIVA_ACPI_NAMESPACE__
#include "libk/hive.h"
#include <libk/stddef.h>

/*
 * Structure that represents a 'node' that can be created and
 * inserted into an acpi AML namespace. It serves as a bridge 
 * into acpiobjects that live at that location in the nodetree
 */
typedef struct {
  // 4 bytes (chars) make up every namespace node in AML
  uint32_t m_identifier;

  // TODO: 
} acpi_node_t;

acpi_node_t* create_acpi_node();
void destroy_acpi_node(acpi_node_t* node);

typedef struct {
  acpi_node_t* m_root;

  // The tree is represented by our own hacky 
  // structure called a hive, which binds entries
  // to a certain unique path. This means duplicates CANT exists
  hive_t* m_nodes;

  // Points to the controlblock where this namespace came from,
  // One namespace can eat up multiple tables worth of AML (yum)
  void* m_table;
  size_t m_table_count;
} acpi_namespace_t;

acpi_namespace_t* create_acpi_namespace();
void destroy_acpi_namespace(acpi_namespace_t* namespace);

/*
 * Adds a node to the namespace at the location this path points to
 */
void acpi_namespace_add_node(acpi_namespace_t* namespace, acpi_node_t* node, const char* path);

/*
 * Adds a node to the namespace inside a given parent. Throws an error
 * if this parent entry does not exist in the namespace.
 */
ErrorOrPtr acpi_namespace_add_node_to_parent(acpi_namespace_t* namespace, acpi_node_t* node, acpi_node_t* parent);

// TODO: we need functions for: managing the nodes in the namespace (removing, adding, ect.), 
// bindings between nodes and acpiobjects or values, ect.

#endif // !__ANIVA_ACPI_NAMESPACE__
