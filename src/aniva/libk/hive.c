#include "hive.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include <libk/string.h>
#include "mem/heap.h"
#include "mem/kmem_manager.h"

hive_t *create_hive(hive_url_part_t root_part) {
  hive_t* ret = kmalloc(sizeof(hive_t));

  if (!ret) {
    return nullptr;
  }

  ret->m_entries = init_list();

  if (!ret->m_entries) {
    kfree(ret);
    return nullptr;
  }

  ret->m_hole_count = 0;

  // FIXME: we are doing some funnie string stuff
  // let's make sure this does not bit our ass lol
  ret->m_url_part = root_part;

  return ret;
}

static hive_entry_t* create_hive_entry(HIVE_ENTRY_TYPE_t type);
static hive_entry_t* __hive_find_entry(hive_t* hive, hive_url_part_t part);
static ANIVA_STATUS __hive_parse_hive_path(hive_t* root, const char* path, ANIVA_STATUS (*fn)(hive_t* root, hive_url_part_t part));
static hive_url_part_t __hive_find_part_at(const char* path, uintptr_t depth);
static size_t __hive_get_part_count(const char* path);
static const char* __hive_prepend_root_part(hive_t* root, const char* path);

ErrorOrPtr hive_add_entry(hive_t* root, void* data, const char* path) {

  if (strcmp(__hive_find_part_at(path, 0), root->m_url_part)) {
    path = __hive_prepend_root_part(root, path);
  }

  //if (hive_get(root, path) != nullptr) {
  //  return Error();
  //}

  hive_entry_t* entry = create_hive_entry(HIVE_ENTRY_TYPE_DATA);
  entry->m_data = data;

  if (!entry) {
    return Error();
  }

  hive_t* current_hive = root;
  size_t part_count = __hive_get_part_count(path);

  println(to_string(part_count));

  // skip the root part
  for (uintptr_t i = 1; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    // end is reached: we need to emplace a data entry
    if (i+1 == part_count) {
      entry->m_entry_part = part;

      list_append(current_hive->m_entries, entry);
      return Success(0);
    }

    hive_entry_t* entry = __hive_find_entry(current_hive, part);

    if (!entry) {
      entry = create_hive_entry(HIVE_ENTRY_TYPE_HOLE);
      entry->m_entry_part = part;
      entry->m_hole = create_hive(part);
    }

    if (hive_entry_is_hole(entry)) {
      current_hive = entry->m_hole;
    }
  }

  return Error();
}

ErrorOrPtr hive_add_hole(hive_t* root, const char* path) {
  hive_t* current_hive;
  size_t part_count = __hive_get_part_count(path);

  for (uintptr_t i = 0; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    if (!strcmp(part, root->m_url_part)) {
      current_hive = root;
      continue;
    }

    hive_entry_t* entry = __hive_find_entry(current_hive, part);

    if (!entry) {
      return Error();
    }

    if (hive_entry_is_hole(entry)) {
      current_hive = entry->m_hole;
    }
  }

  return Success(0);
}

void hive_add_holes(hive_t* root, const char* path) {
  hive_t* current_hive;
  size_t part_count = __hive_get_part_count(path);

  for (uintptr_t i = 0; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    if (!strcmp(part, root->m_url_part)) {
      current_hive = root;
      continue;
    }

    hive_entry_t* entry = __hive_find_entry(current_hive, part);

    if (!entry) {
      entry = create_hive_entry(HIVE_ENTRY_TYPE_HOLE);
      entry->m_entry_part = part;
      entry->m_hole = create_hive(part);
    }

    if (hive_entry_is_hole(entry)) {
      current_hive = entry->m_hole;
    }
  }
}

void* hive_get(hive_t* root, const char* path) {

  hive_t* current_hive;
  size_t part_count = __hive_get_part_count(path);

  println(path);
  println(to_string(part_count));

  for (uintptr_t i = 0; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    if (!strcmp(part, root->m_url_part)) {
      current_hive = root;
      continue;
    }

    println(current_hive->m_url_part);
    println(part);

    hive_entry_t* entry = __hive_find_entry(current_hive, part);

    if (!entry) {
      println("nullptr");
      return nullptr;
    }

    if (i+1 == part_count) {
      return entry->m_data;
    }

    if (hive_entry_is_hole(entry)) {
      current_hive = entry->m_hole;
    }
  }

  return nullptr;
}

const char* hive_get_path(hive_t* root, void* data) {


  return nullptr;
}

ErrorOrPtr hive_walk(hive_t* root, void (*itterate_fn)(void* hive, void* data)) {

  FOREACH(i, root->m_entries) {
    hive_entry_t* entry = i->data;

    if (hive_entry_is_hole(entry)) {
      hive_walk(entry->m_hole, itterate_fn);
    } else {
      itterate_fn(root, entry);
    }
  }

  return Success(0);
}

/*
 * Static functions
 */

static hive_entry_t* create_hive_entry(HIVE_ENTRY_TYPE_t type) {
  hive_entry_t* ret = kmalloc(sizeof(hive_entry_t));

  if (!ret) {
    return nullptr;
  }

  ret->m_type = type;
  ret->m_data = nullptr;

  return ret;
}

static hive_entry_t* __hive_find_entry(hive_t* hive, hive_url_part_t part) {

  if (!hive) {
    return nullptr;
  }

  FOREACH(i, hive->m_entries) {
    hive_entry_t* entry = i->data;

    if (!strcmp(entry->m_entry_part, part)) {
      return entry;
    }
  }

  return nullptr;
}

static ANIVA_STATUS __hive_parse_hive_path(hive_t* root, const char* path, ANIVA_STATUS (*fn)(hive_t* root, hive_url_part_t part)) {
  size_t hive_part_count = 0;
  size_t path_length = strlen(path);

  const char* current_part_ptr;
  size_t current_part_length = 0;

  bool found_entire_part = false;

  for (uintptr_t i = 0; i < path_length; i++) {
    char c = path[i];

    if (c == HIVE_PART_SEPERATOR) {
      found_entire_part = true;
      hive_part_count++;

      // construct the part
      hive_url_part_t part = kmalloc(current_part_length + 1);
      memset(part, 0, current_part_length + 1);
      strcpy(part, current_part_ptr);
      if (fn(root, part) != ANIVA_SUCCESS) {
        kfree(part);
        return ANIVA_FAIL;
      }
      kfree(part);
    } else {
      // we found a seperator previously, 
      // so we are starting a new part
      if (found_entire_part) {
        current_part_ptr = &path[i];
        found_entire_part = false;
      }
      current_part_length++;
    }
  }

  return ANIVA_SUCCESS;
}

static hive_url_part_t __hive_find_part_at(const char* path, uintptr_t depth) {
  size_t current_depth = 0;
  size_t path_length = strlen(path);

  hive_url_part_t ret = nullptr;

  if (depth == 0) {
    uintptr_t i = 0;
    while (path[i] != HIVE_PART_SEPERATOR && path[i] != '\0') {
      i++;
    }
    ret = kmalloc(i + 1);
    strcpy(ret, &path[0]);
    ret[i] = '\0';
    return ret;
  }

  for (uintptr_t i = 0; i < path_length; i++) {
    char c = path[i];

    if (c == HIVE_PART_SEPERATOR) {
      current_depth++;
      continue;
    } 

    if (current_depth == depth) {
      uintptr_t j = 0;
      while (path[i+j] != HIVE_PART_SEPERATOR && path[i+j] != '\0') {
        j++;
      }
      ret = kmalloc(j + 1);
      strcpy(ret, &path[i]);
      ret[j] = '\0';
      return ret;
    }
  }

  return nullptr;
}

static size_t __hive_get_part_count(const char* path) {
  size_t part_count = 1;
  size_t path_length = strlen(path);

  for (uintptr_t i = 0; i < path_length; i++) {
    char c = path[i];

    if (c == HIVE_PART_SEPERATOR) {
      part_count++;
    }
  }
  return part_count;
}

static const char* __hive_prepend_root_part(hive_t* root, const char* path) {
  if (!strcmp(root->m_url_part, __hive_find_part_at(path, 0))) {
    return path;
  }

  const size_t extended_path_size = strlen(root->m_url_part) + 1 + strlen(path) + 1;

  char* extended_path = kmalloc(extended_path_size);

  bool prepending_root = true;

  uintptr_t extended_index = 0;

  strcpy(extended_path, root->m_url_part);
  extended_index += strlen(root->m_url_part);
  extended_path[extended_index] = HIVE_PART_SEPERATOR;
  extended_index++;
  strcpy(&extended_path[extended_index], path);
  extended_index += strlen(path);
  extended_path[extended_index] = '\0';

  println(extended_path);
  return extended_path;
}
