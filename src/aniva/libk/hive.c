#include "hive.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include <libk/string.h>
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "sched/scheduler.h"

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


static hive_entry_t* create_hive_entry(hive_t* hole, void* data, hive_url_part_t part);
static void destroy_hive_entry(hive_entry_t* entry);
static hive_entry_t* __hive_find_entry(hive_t* hive, hive_url_part_t part);
static ANIVA_STATUS __hive_parse_hive_path(hive_t* root, const char* path, ANIVA_STATUS (*fn)(hive_t* root, hive_url_part_t part));
static hive_url_part_t __hive_find_part_at(const char* path, uintptr_t depth);
static size_t __hive_get_part_count(const char* path);
static const char* __hive_prepend_root_part(hive_t* root, const char* path);

void destroy_hive(hive_t* hive) {

  FOREACH(i, hive->m_entries) {
    hive_entry_t* entry = i->data;

    if (entry) {
      destroy_hive_entry(entry);
    }
  }

  destroy_list(hive->m_entries);
  kfree(hive->m_url_part);
  kfree(hive);
}

static bool __hive_path_is_invalid(const char* path);

ErrorOrPtr hive_add_entry(hive_t* root, void* data, const char* path) {

  path = __hive_prepend_root_part(root, path);

  if (__hive_path_is_invalid(path)) {
    return Error();
  }

  if (hive_get(root, path) != nullptr) {
    return Error();
  }

  hive_entry_t* new_entry = create_hive_entry(nullptr, data, nullptr);
  if (!new_entry) {
    return Error();
  }

  hive_t* current_hive = root;
  size_t part_count = __hive_get_part_count(path);

  // skip the root part
  for (uintptr_t i = 1; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    // end is reached: we need to emplace a data entry
    if (i+1 == part_count) {
      new_entry->m_entry_part = part;

      // Increment stats
      // TODO: stats are widely missing from 
      // the functions where they should be implemented
      current_hive->m_total_entry_count++;
      root->m_total_entry_count++;

      list_append(current_hive->m_entries, new_entry);
      return Success(0);
    }

    bool created_new_entry = false;
    hive_entry_t* current_entry = __hive_find_entry(current_hive, part);

    // Don't get rid of part here, we still need it
    if (!current_entry) {
      // Append a new hole
      created_new_entry = true;
      current_entry = create_hive_entry(create_hive(part), nullptr, part);
      list_append(current_hive->m_entries, current_entry);
      // Fallthrough
    }

    if (hive_entry_is_hole(current_entry)) {
      current_hive = current_entry->m_hole;
    } else {
      current_entry->m_hole = create_hive(part);
      current_hive = current_entry->m_hole;
    }

    if (!created_new_entry)
      kfree(part);
  }

  return Error();
}

ErrorOrPtr hive_add_hole(hive_t* root, const char* path, hive_t* hole) {

  path = __hive_prepend_root_part(root, path);

  if (__hive_path_is_invalid(path)) {
    return Error();
  }

  if (hive_get(root, path) != nullptr) {
    return Error();
  }

  hive_entry_t* new_entry = create_hive_entry(hole, nullptr, nullptr);
  if (!new_entry) {
    return Error();
  }

  hive_t* current_hive = root;
  size_t part_count = __hive_get_part_count(path);

  // skip the root part
  for (uintptr_t i = 1; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    // end is reached: we need to emplace a data entry
    if (i+1 == part_count) {
      new_entry->m_entry_part = part;

      // Increment stats
      // TODO: stats are widely missing from 
      // the functions where they should be implemented
      current_hive->m_total_entry_count++;
      root->m_total_entry_count++;

      list_append(current_hive->m_entries, new_entry);
      return Success(0);
    }

    bool created_new_entry = false;
    hive_entry_t* current_entry = __hive_find_entry(current_hive, part);

    // Don't get rid of part here, we still need it
    if (!current_entry) {
      // Append a new hole
      created_new_entry = true;
      current_entry = create_hive_entry(create_hive(part), nullptr, part);
      list_append(current_hive->m_entries, current_entry);
      // Fallthrough
    }

    if (hive_entry_is_hole(current_entry)) {
      current_hive = current_entry->m_hole;
    } else {
      current_entry->m_hole = create_hive(part);
      current_hive = current_entry->m_hole;
    }

    if (!created_new_entry)
      kfree(part);
  }

  return Error();
}

ErrorOrPtr hive_add_holes(hive_t* root, const char* path) {

  kernel_panic("UNIMPLEMENTED hive_add_holes");
  return Success(0);
}

void hive_set(hive_t* root, void* data, const char* path) {

  path = __hive_prepend_root_part(root, path);

  if (__hive_path_is_invalid(path)) {
    goto invalid_path;
  }

  hive_t* current_hive = root;
  size_t part_count = __hive_get_part_count(path);

  for (uintptr_t i = 1; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    hive_entry_t* entry = __hive_find_entry(current_hive, part);
    kfree(part);

    if (!entry) {
      // NOTE: freeing path here seems to upset some Must() call =/ so yea FIXME
      goto invalid_path;
    }

    if (i+1 == part_count) {
      kfree((void*)path);
      entry->m_data = data;
    }

    if (hive_entry_is_hole(entry)) {
      current_hive = entry->m_hole;
    }
  }

invalid_path:
  kfree((void*)path);
}

void* hive_get(hive_t* root, const char* path) {

  path = __hive_prepend_root_part(root, path);

  if (__hive_path_is_invalid(path)) {
    goto invalid_path;
  }

  hive_t* current_hive = root;
  size_t part_count = __hive_get_part_count(path);

  for (uintptr_t i = 1; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    hive_entry_t* entry = __hive_find_entry(current_hive, part);
    kfree(part);

    if (!entry) {
      // NOTE: freeing path here seems to upset some Must() call =/ so yea FIXME
      return nullptr;
    }

    if (i+1 == part_count) {
      kfree((void*)path);
      return entry->m_data;
    }

    if (hive_entry_is_hole(entry)) {
      current_hive = entry->m_hole;
    }
  }

invalid_path:
  kfree((void*)path);
  return nullptr;
}

ErrorOrPtr hive_remove(hive_t* root, void* data) {
  if (!hive_contains(root, data)) {
    return Error();
  }

  const char* path = hive_get_path(root, data);

  return hive_remove_path(root, path);
}

ErrorOrPtr hive_remove_path(hive_t* root, const char* path) {
  // TODO:
  path = __hive_prepend_root_part(root, path);

  if (__hive_path_is_invalid(path)) {
    return Error();
  }

  if (hive_get(root, path) == nullptr) {
    return Error();
  }

  hive_t* current_hive = root;
  size_t part_count = __hive_get_part_count(path);

  for (uintptr_t i = 1; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path, i);

    hive_entry_t* entry = __hive_find_entry(current_hive, part);
    bool is_hole = hive_entry_is_hole(entry);

    kfree(part);

    if (!entry) {
      return Error();
    }

    if (i+1 == part_count) {
      kfree(part);
      if (is_hole && entry->m_hole->m_entries->m_length != 0) {
        return Error();
      }

      uintptr_t index = Release(list_indexof(current_hive->m_entries, entry));
      list_remove(current_hive->m_entries, index);
      return Success(0);
    }

    if (hive_entry_is_hole(entry)) {
      current_hive = entry->m_hole;
    }
  }

  return Error();
}

// root.hole.entry
const char* hive_get_path(hive_t* root, void* data) {

  //if (!hive_contains(root, data)) {
  //  return nullptr;
  //}

  char* ret = nullptr;

  FOREACH(i, root->m_entries) {
    hive_entry_t* entry = i->data;

    if (hive_entry_is_hole(entry)) {
      const char* hole_path = hive_get_path(entry->m_hole, data);

      if (hole_path != nullptr) {
        // append
        ret = (char*)__hive_prepend_root_part(root, hole_path);
        kfree((void*)hole_path);
        return ret;
      }
    }

    if (entry->m_data == data) {
      // append
      ret = (char*)__hive_prepend_root_part(root, entry->m_entry_part);
      return ret;
    }
  }
  return nullptr;
}

void* hive_get_relative(hive_t* root, const char* subpath) {
  kernel_panic("UNIMPLEMENTED hive_get_relative");
  return nullptr;
}

bool hive_contains(hive_t* root, void* data) {

  FOREACH(i, root->m_entries) {
    hive_entry_t* entry = i->data;

    if (hive_entry_is_hole(entry)) {
      if (hive_contains(entry->m_hole, data)) {
        return true;
      }
    }

    if (!entry->m_data)
      continue;

    if (entry->m_data == data)
      return true;
  }

  return false;
}

ErrorOrPtr hive_walk(hive_t* root, bool (*itterate_fn)(hive_t* hive, void* data)) {

  FOREACH(i, root->m_entries) {
    hive_entry_t* entry = i->data;

    if (hive_entry_is_hole(entry)) {
      TRY(res, hive_walk(entry->m_hole, itterate_fn));
    }

    if (!entry->m_data)
      continue;

    if (!itterate_fn(root, entry->m_data))
      return Error();
  }

  return Success(0);
}

/*
 * Static functions
 */

static hive_entry_t* create_hive_entry(hive_t* hole, void* data, hive_url_part_t part) {
  hive_entry_t* ret = kmalloc(sizeof(hive_entry_t));

  if (!ret) {
    return nullptr;
  }

  // ret->m_type = type;
  ret->m_data = data;
  ret->m_hole = hole;
  ret->m_entry_part = part;

  return ret;
}

static void destroy_hive_entry(hive_entry_t* entry) {

  // recursively kill off nested hives
  if (hive_entry_is_hole(entry))
    destroy_hive(entry->m_hole);

  kfree(entry->m_entry_part);
  kfree(entry);
}

static hive_entry_t* __hive_find_entry(hive_t* hive, hive_url_part_t part) {

  if (!hive) {
    return nullptr;
  }

  const size_t part_length = strlen(part);

  FOREACH(i, hive->m_entries) {
    hive_entry_t* entry = i->data;

    const size_t check_length = strlen(entry->m_entry_part);

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

  for (uintptr_t i = 0; i < path_length; i++) {
    char c = path[i];

    if (current_depth == depth) {
      uintptr_t j = 0;
      while (path[i+j] != HIVE_PART_SEPERATOR && path[i+j] != '\0') {
        j++;
      }
      ret = kmalloc(j + 1);
      memcpy(ret, path + i, j);
      ret[j] = '\0';
      return ret;
    }

    if (c == HIVE_PART_SEPERATOR) {
      current_depth++;
      continue;
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

  hive_url_part_t root_part = __hive_find_part_at(path, 0);

  if (!strcmp(root->m_url_part, root_part)) {
    kfree(root_part);
    return path;
  }

  kfree(root_part);

  const size_t extended_path_size = strlen(root->m_url_part) + 1 + strlen(path) + 1;

  char* extended_path = kmalloc(extended_path_size);

  uintptr_t extended_index = 0;

  strcpy(extended_path, root->m_url_part);
  extended_index += strlen(root->m_url_part);
  extended_path[extended_index] = HIVE_PART_SEPERATOR;
  extended_index++;
  strcpy(&extended_path[extended_index], path);
  extended_index += strlen(path);
  extended_path[extended_index] = '\0';

  return extended_path;
}

/*
 * Invalid if:
 *  - two seperators after eachother
 *  - starts or ends with a seperator
 *  - it has no parts
 *  - it has a length of 0 (-_-)
 *  - TODO: more
 */
static bool __hive_path_is_invalid(const char* path) {
  const size_t part_count = __hive_get_part_count(path);
  const size_t path_length = strlen(path);
  hive_url_part_t current_part;

  if (part_count == 0 || path_length == 0) {
    return true;
  }

  if (path[0] == HIVE_PART_SEPERATOR || path[path_length] == HIVE_PART_SEPERATOR) {
    return true;
  }

  for (uintptr_t i = 0; i < part_count; i++) {
    current_part = __hive_find_part_at(path, i);

    if (current_part == nullptr) {
      goto fail;
    }
    kfree(current_part);
  }

  return false;

fail:
    kfree(current_part);
    return true;
}
