#include "namespace.h"
#include "dev/debug/serial.h"
#include "dev/framebuffer/framebuffer.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include "mem/heap.h"
#include "libk/string.h"
#include "mem/kmem_manager.h"
#include "system/acpi/parser.h"

acpi_aml_seg_t *acpi_load_segment(void* table_ptr, int idx) {
  acpi_aml_seg_t *ret = kmalloc(sizeof(acpi_aml_seg_t));
  memset(ret, 0, sizeof(*ret));

  ret->aml_table = table_ptr;
  ret->idx = idx;

  println(to_string((uintptr_t) table_ptr));
  kmem_kernel_alloc((uintptr_t)ret->aml_table, sizeof(acpi_aml_table_t), KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);

  kmem_kernel_alloc((uintptr_t)ret->aml_table->data, ret->aml_table->header.length, KMEM_CUSTOMFLAG_PERSISTANT_ALLOCATE);
  return ret;
}

acpi_ns_node_t *acpi_create_root() {
  acpi_ns_node_t* root_node = acpi_create_node();
  root_node->type = ACPI_NAMESPACE_ROOT;
  root_node->parent = nullptr;
  memcpy(&root_node->name, "\\___", 4);

  g_parser_ptr->m_ns_root_node = root_node;
  
  acpi_ns_node_t* sb_node = acpi_create_node();
  sb_node->type = ACPI_NAMESPACE_DEVICE;
  sb_node->parent = root_node;
  memcpy(&sb_node->name, "_SB_", 4);

  acpi_ns_node_t* si_node = acpi_create_node();
  si_node->type = ACPI_NAMESPACE_DEVICE;
  si_node->parent = root_node;
  memcpy(&si_node->name, "_SI_", 4);

  acpi_ns_node_t* gpe_node = acpi_create_node();
  gpe_node->type = ACPI_NAMESPACE_DEVICE;
  gpe_node->parent = root_node;
  memcpy(&gpe_node->name, "_GPE", 4);

  acpi_ns_node_t* pr_node = acpi_create_node();
  pr_node->type = ACPI_NAMESPACE_DEVICE;
  pr_node->parent = root_node;
  memcpy(&pr_node->name, "_PR_", 4);

  acpi_ns_node_t* tz_node = acpi_create_node();
  tz_node->type = ACPI_NAMESPACE_DEVICE;
  tz_node->parent = root_node;
  memcpy(&tz_node->name, "_TZ_", 4);

  acpi_ns_node_t* osi_node = acpi_create_node();
  osi_node->type = ACPI_NAMESPACE_METHOD;
  osi_node->parent = root_node;
  // TODO: do override and flags
  osi_node->aml_method_override = nullptr;
  memcpy(&osi_node->name, "_OSI", 4);

  acpi_ns_node_t* os_node = acpi_create_node();
  os_node->type = ACPI_NAMESPACE_METHOD;
  os_node->parent = root_node;
  // TODO: do override and flags
  os_node->aml_method_override = nullptr;
  memcpy(&os_node->name, "_OS_", 4);
  
  acpi_ns_node_t* rev_node = acpi_create_node();
  rev_node->type = ACPI_NAMESPACE_METHOD;
  rev_node->parent = root_node;
  // TODO: do override and flags
  rev_node->aml_method_override = nullptr;
  memcpy(&rev_node->name, "_REV", 4);

  // install the nodes into the parser-tree
  acpi_load_ns_node_in_parser(g_parser_ptr, sb_node);
  acpi_load_ns_node_in_parser(g_parser_ptr, si_node);
  acpi_load_ns_node_in_parser(g_parser_ptr, gpe_node);
  acpi_load_ns_node_in_parser(g_parser_ptr, pr_node);
  acpi_load_ns_node_in_parser(g_parser_ptr, tz_node);
  acpi_load_ns_node_in_parser(g_parser_ptr, osi_node);
  acpi_load_ns_node_in_parser(g_parser_ptr, os_node);
  acpi_load_ns_node_in_parser(g_parser_ptr, rev_node);

  return root_node;
}

acpi_ns_node_t* acpi_create_node() {
  acpi_ns_node_t* ret = kmalloc(sizeof(acpi_ns_node_t));
  if (ret) {
    memset(ret, 0, sizeof(acpi_ns_node_t));
  }
  return ret;
}

acpi_ns_node_t *acpi_get_root(acpi_parser_t* parser) {
  return parser->m_ns_root_node;
}

int acpi_parse_aml_name(acpi_aml_name_t* name, const uint8_t* data) {
  if (!name) {
    return -1;
  }

  memset(name, 0, sizeof(acpi_aml_name_t));

  const uint8_t *start = data;
  const uint8_t *itteration = start;

  // absolute path
  if (*itteration == '\\') {
    name->m_absolute = true;
    itteration++;
  } else {
    while (*itteration == '^') {
      name->m_scope_height++;
      itteration++;
    }
  }

  uint32_t segs = 0;
  if (*itteration == '\0') {
    itteration++;
    segs = 0;
  } else if (*itteration == DUAL_PREFIX) {
    itteration++;
    segs = 2;
  } else if (*itteration == MULTI_PREFIX) {
    itteration++;
    segs = *itteration;
    itteration++;
  } else {
    if (!acpi_is_name(*itteration)) {
      return -1;
    }
    segs = 1;
  }

  name->m_should_search_parent_scopes = (name->m_absolute == false && name->m_scope_height == 0 && segs == 1);
  name->m_start_p = start;
  name->m_itterator_p = itteration;
  name->m_end_p = itteration + 4 * segs;
  name->m_size = name->m_end_p - name->m_start_p;
  return 0;
}

char* acpi_aml_name_to_string(acpi_aml_name_t* name) {
  acpi_aml_name_t name_cpy = *name;
  
  size_t segment_num = (name_cpy.m_end_p - name_cpy.m_itterator_p) / 4;
  size_t max_length = 1 + name_cpy.m_scope_height + segment_num * 5 + 1;

  char* str = kmalloc(max_length);

  int n = 0;
  if (name_cpy.m_absolute)
      str[n++] = '\\';

  for (int i = 0; i < name_cpy.m_scope_height; i++)
      str[n++] = '^';

  if (name_cpy.m_itterator_p != name_cpy.m_end_p) {
    for (;;) {
      acpi_aml_name_next_segment(&name_cpy, &str[n]);
      n += 4;
      if (name_cpy.m_itterator_p == name_cpy.m_end_p)
          break;
      str[n++] = '.';
    }
  }
  str[n++] = '\0';

  if (n != (int)max_length) {
    char* new_buffer = kmalloc(n);
    memcpy(new_buffer, str, n);
    kfree(str);
    str = new_buffer;
  }
  return str;
}

void acpi_load_ns_node_in_parser(struct acpi_parser* parser, acpi_ns_node_t* node) {
  // add to the parsers array
  if (parser->m_ns_size == parser->m_ns_max_size) {
    // realloc
    size_t new_max_size = parser->m_ns_max_size * 2;
    if (!new_max_size) {
      new_max_size = 128;
    }
    acpi_ns_node_t** new_array = kmalloc(sizeof(acpi_ns_node_t*) * new_max_size);
    if (parser->m_ns_nodes != nullptr) {
      memcpy(new_array, parser->m_ns_nodes, sizeof(acpi_ns_node_t*) * parser->m_ns_max_size);
      kfree(parser->m_ns_nodes);
    }

    parser->m_ns_nodes = new_array;
    parser->m_ns_max_size = new_max_size;
  }

  println("installing node");

  parser->m_ns_nodes[parser->m_ns_size] = node;
  parser->m_ns_size++;

  // add to the list of its parent

  acpi_ns_node_t* parent = node->parent;

  if (parent) {
    if (ns_node_has_child(parent, node)) {
      println("WARNING: trying to load duplicate ns node!");
      return;
    }
    ns_node_insert_child(parent, node);
  }
}

void acpi_unload_ns_node_in_parser(struct acpi_parser* parser, acpi_ns_node_t* node) {
  // remove from the parsers array

  // remove from the list of its parent
}

acpi_ns_node_t *acpi_get_node_child(acpi_ns_node_t* handle, const char* name) {
  // make sure to keep the handle pointer alive
  acpi_ns_node_t* handle_p_cpy = handle;

  if (!handle_p_cpy) {
    handle_p_cpy = acpi_get_root(g_parser_ptr);
  }
  
  return ns_node_get_child_by_name(handle_p_cpy, name);
}

acpi_ns_node_t* acpi_resolve_node(acpi_ns_node_t* handle, acpi_aml_name_t* aml_name) {
  acpi_aml_name_t name_copy = *aml_name;
  acpi_ns_node_t* current_handle = handle;

  if (name_copy.m_should_search_parent_scopes) {
    println("searching parent scopes");
    // there is only one segment in this name
    char segment[5] = {0};
    acpi_aml_name_next_segment(&name_copy, segment);
    ASSERT_MSG(name_copy.m_itterator_p == name_copy.m_end_p, "acpi_resolve_node: more than one segment in the aml name!");
    segment[4] = '\0';

    while (current_handle) {
      acpi_ns_node_t* current_child = acpi_get_node_child(current_handle, segment);

      if (!current_child) {
        current_handle = current_handle->parent;
        continue;
      }

      return current_child;
    }

    return nullptr;
  }

  if (name_copy.m_absolute) {
    // search from the namespace root
    while (current_handle->parent) {
      current_handle = current_handle->parent;
    }
  }

  for (uint32_t i = 0; i < name_copy.m_scope_height; i++) {
    if (!current_handle->parent) {
      break;
    }
    current_handle = current_handle->parent;
  }

  while (name_copy.m_itterator_p != name_copy.m_end_p) {
    char segment[4];
    acpi_aml_name_next_segment(&name_copy, segment);

    putch(segment[0]);
    putch(segment[1]);
    putch(segment[2]);
    putch(segment[3]);
    putch('\n');
    current_handle = acpi_get_node_child(current_handle, segment);
    if (!current_handle) {
      return nullptr;
    }
  }

  // check for namespace aliases
  if (current_handle->type == ACPI_NAMESPACE_ALIAS) {
    current_handle = current_handle->namespace_alias_target;
    ASSERT_MSG(current_handle->type != ACPI_NAMESPACE_ALIAS, "Found a namespace alias to another namespace alias");
  }

  return current_handle;
}

void acpi_resolve_new_node(acpi_ns_node_t *node, acpi_ns_node_t* handle, acpi_aml_name_t* aml_name) {
  acpi_aml_name_t name_copy = *aml_name;
  acpi_ns_node_t* current_handle = handle;

  if (name_copy.m_absolute) {
    // search from the namespace root
    while (current_handle->parent) {
      current_handle = current_handle->parent;
    }
  }

  for (uint32_t i = 0; i < name_copy.m_scope_height; i++) {
    if (!current_handle->parent) {
      break;
    }
    current_handle = current_handle->parent;
  }

  while (true) {
    char segment[5];
    acpi_aml_name_next_segment(&name_copy, segment);
    segment[4] = '\0';

    if (name_copy.m_itterator_p == name_copy.m_end_p) {
      memcpy(&node->name, segment, 4);
      node->parent = current_handle;
      break;
    } else {
      current_handle = acpi_get_node_child(current_handle, segment);
      if (current_handle && current_handle->type == ACPI_NAMESPACE_ALIAS) {
        // fuck man
        current_handle = current_handle->namespace_alias_target;
      }
    }
  } 

}

bool ns_node_has_child(acpi_ns_node_t* handle, acpi_ns_node_t* node) {
  FOREACH(i, handle->m_children) {
    acpi_ns_node_t* child = i->data;
    if (memcmp(&child->name, &node->name, 4)) {
      return true;
    }
  }
  return false;
}

void ns_node_insert_child(acpi_ns_node_t* handle, acpi_ns_node_t* child) {

  list_append(handle->m_children, child);
}

void ns_node_remove_child(acpi_ns_node_t* handle, acpi_ns_node_t* child) {
  // TODO: now this is where our implementation kinda sucks =)
}

acpi_ns_node_t* ns_node_get_child_by_name(acpi_ns_node_t* handle, const char* name) {
  print("looking for: ");
  println(name);
  println("children: ");
  FOREACH(i, handle->m_children) {
    acpi_ns_node_t* child = i->data;
    if (memcmp(&child->name, name, 4)) {
      return child;
    }
  }

  println("no child found");
  return nullptr;
}
