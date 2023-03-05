#include "hive.h"
#include "libk/error.h"
#include "libk/linkedlist.h"
#include <libk/string.h>
#include "mem/heap.h"

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
static hive_entry_t* find_entry(hive_t* hive, hive_url_part_t part);
static void parse_hive_path(const char* path, void (*fn)(hive_url_part_t part));


ErrorOrPtr hive_add_entry(hive_t* hive, void* data, hive_url_part_t part) {

  if (find_entry(hive, part) != nullptr) {
    return Error();
  }

  hive_entry_t* entry = create_hive_entry(HIVE_ENTRY_TYPE_DATA);

  if (!entry) {
    return Error();
  }

  entry->m_data = data;
  entry->m_entry_part = part;

  list_append(hive->m_entries, entry);

  return Success(0);
}

ErrorOrPtr hive_add_hole(hive_t* root, const char* path) {

  hive_entry_t* entry = create_hive_entry(HIVE_ENTRY_TYPE_HOLE);

  if (!entry) {
    return Error();
  }



  return Success(0);
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

static hive_entry_t* find_entry(hive_t* hive, hive_url_part_t part) {

  if (!hive) {
    return nullptr;
  }

  FOREACH(i, hive->m_entries) {
    hive_entry_t* entry = i->data;

    if (strcmp(entry->m_entry_part, part)) {
      return entry;
    }
  }

  return nullptr;
}

static void parse_hive_path(const char* path, void (*fn)(hive_url_part_t part)) {
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
      fn(part);
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
}


