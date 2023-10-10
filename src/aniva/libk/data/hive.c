#include "hive.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "libk/data/linkedlist.h"
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
  //kfree(hive->m_url_part);
  kfree(hive);
}

static bool __hive_path_is_invalid(const char* path);

ErrorOrPtr hive_add_entry(hive_t* root, void* data, const char* path) {

  path = __hive_prepend_root_part(root, path);

  if (__hive_path_is_invalid(path)) {
    return Error();
  }

  /* When the entry already exists, we don't really want to instantly die yk */
  if (hive_get(root, path) != nullptr) {
    return Warning();
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

/*
 * The new rewritten get algorithm for the hive (basically a tree) implementation
 *
 * A hive is basically a linkedlist that contains linkedlists, for better data grouping.
 * It's probably technically called something else, but this kind 'hive' had a different kind
 * of function when I first designed it. It then got rewritten and rewritten and now it's
 * just basically a tree lmao
 * 
 * Anyhow, this hive_get function simply walks the path string that is provided and cuts it 
 * up where there are HIVE_PART_SEPERATOR symbols (most likely '/'). It walks the hive structure
 * at the same time as it does the path and finds the matching entry per subpath that gets extracted
 * (a subpath is every part of the path that is inbetween HIVE_PART_SEPERATOR symbols, or at the start 
 * (the root) or at the end (the tail)). When we reach the end of the path, we can assume that 
 * the data is supposed to be here.
 *
 * The current implementation of hive_t has a lot of unnecessary heap usage, which I want to slowly phase out
 */
void* hive_get(hive_t* root, const char* path) 
{
  char* end_ptr;
  char* start_ptr;
  char buffer[strlen(path) + 1];
  hive_t* current;
  hive_entry_t* current_entry;
  memcpy(buffer, path, strlen(path) + 1);

  start_ptr = buffer;
  end_ptr = buffer;
  current = root;
  current_entry = nullptr;

  /* Loop one, to look for the root pathpart */
  while (*end_ptr) {
    if (*end_ptr != HIVE_PART_SEPERATOR) {
      end_ptr++;
      continue;
    }

    /* Preemptive null-terminate */
    *end_ptr = '\0';

    /* Check if we are indeed in the root */
    if (strcmp(root->m_url_part, start_ptr) != 0) {
      /* Nope, let's fix this up */
      *end_ptr = HIVE_PART_SEPERATOR;
      start_ptr = end_ptr = buffer;
      break;
    }
    
    /* We are, make sure that we actually skip this part in the path */
    end_ptr++;
    start_ptr = end_ptr;
    break;
  }

  /* Loop two, to resolve the rest of the path */
  while (*end_ptr) {

    if (*end_ptr != HIVE_PART_SEPERATOR)
      goto cycle;

    /* Temporarily terminate the string */
    *end_ptr = '\0';

    FOREACH(i, current->m_entries) {
      current_entry = i->data;

      if (!hive_entry_is_hole(current_entry))
        continue;

      if (strcmp(current_entry->m_entry_part, start_ptr) == 0) {
        current = current_entry->m_hole;
        break;
      }

      current_entry = nullptr;
    }

    /* When the linear scan is done, we drop down here */
    if (!current_entry)
      return nullptr;

    *end_ptr = HIVE_PART_SEPERATOR;
    start_ptr = end_ptr + 1;
cycle:
    end_ptr++;
  }

  if (!current)
    return nullptr;

  FOREACH(i, current->m_entries) {
    current_entry = i->data;

    if (strcmp(current_entry->m_entry_part, start_ptr) == 0) {
      return current_entry->m_data;
    }
  }

  return nullptr;
}

void* hive_get_old(hive_t* root, const char* path) {

  /* Some epic invalid paths
   TODO: see if these checks need to go anywhere else */
  if (!path || !*path)
    return nullptr;

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

ErrorOrPtr hive_remove_path(hive_t* root, const char* path)
{
  uint32_t index;
  char* end_ptr;
  char* start_ptr;
  char buffer[strlen(path) + 1];
  hive_t* current;
  hive_entry_t* current_entry;
  memcpy(buffer, path, strlen(path) + 1);

  start_ptr = buffer;
  end_ptr = buffer;
  current = root;
  current_entry = nullptr;

  /* Loop one, to look for the root pathpart */
  while (*end_ptr) {
    if (*end_ptr != HIVE_PART_SEPERATOR) {
      end_ptr++;
      continue;
    }

    /* Preemptive null-terminate */
    *end_ptr = '\0';

    /* Check if we are indeed in the root */
    if (strcmp(root->m_url_part, start_ptr) != 0) {
      /* Nope, let's fix this up */
      *end_ptr = HIVE_PART_SEPERATOR;
      start_ptr = end_ptr = buffer;
      break;
    }
    
    /* We are, make sure that we actually skip this part in the path */
    end_ptr++;
    start_ptr = end_ptr;
    break;
  }

  /* Loop two, to resolve the rest of the path */
  while (*end_ptr) {

    if (*end_ptr != HIVE_PART_SEPERATOR)
      goto cycle;

    /* Temporarily terminate the string */
    *end_ptr = '\0';

    FOREACH(i, current->m_entries) {
      current_entry = i->data;

      if (!hive_entry_is_hole(current_entry))
        continue;

      if (strcmp(current_entry->m_entry_part, start_ptr) == 0) {
        current = current_entry->m_hole;
        break;
      }

      current_entry = nullptr;
    }

    /* When the linear scan is done, we drop down here */
    if (!current_entry)
      return Error();

    *end_ptr = HIVE_PART_SEPERATOR;
    start_ptr = end_ptr + 1;
cycle:
    end_ptr++;
  }

  if (!current)
    return Error();

  index = 0;

  FOREACH(i, current->m_entries) {
    current_entry = i->data;

    /* Found it! */
    if (strcmp(current_entry->m_entry_part, start_ptr) == 0) {

      list_remove(current->m_entries, index);

      destroy_hive_entry(current_entry);

      return Success(0);
    }

    index++;
  }

  return Error();

  /*
  uintptr_t index;
  size_t part_count;
  hive_t* current_hive;

  if (!root || !path)
    return Error();

  const char* path_cpy = __hive_prepend_root_part(root, path);

  if (__hive_path_is_invalid(path_cpy))
    goto exit;

  if (hive_get(root, path_cpy) == nullptr)
    goto exit;

  index = NULL;
  part_count = __hive_get_part_count(path_cpy);
  current_hive = root;

  for (uintptr_t i = 1; i < part_count; i++) {
    hive_url_part_t part = __hive_find_part_at(path_cpy, i);

    hive_entry_t* entry = __hive_find_entry(current_hive, part);
    bool is_hole = hive_entry_is_hole(entry);

    kfree(part);

    if (!entry)
      goto exit;

    if (i+1 == part_count) {
      kfree(part);

      if (is_hole && entry->m_hole->m_entries->m_length != 0)
        goto exit;

      index = Release(list_indexof(current_hive->m_entries, entry));
      list_remove(current_hive->m_entries, index);

      destroy_hive_entry(entry);

      kfree((void*)path_cpy);
      return Success(0);
    }

    if (hive_entry_is_hole(entry))
      current_hive = entry->m_hole;
  }

exit:
  kfree((void*)path_cpy);
  return Error();
  */
}

// root.hole.entry
const char* hive_get_path(hive_t* root, void* data) 
{
  char* ret = nullptr;

  FOREACH(i, root->m_entries) {
    hive_entry_t* entry = i->data;

    if (hive_entry_is_hole(entry)) {
      const char* hole_path = hive_get_path(entry->m_hole, data);

      if (hole_path) {
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

ErrorOrPtr hive_walk(hive_t* root, bool recursive, bool (*itterate_fn)(hive_t* hive, void* data)) {

  FOREACH(i, root->m_entries) {
    hive_entry_t* entry = i->data;

    if (hive_entry_is_hole(entry) && recursive) {
      TRY(res, hive_walk(entry->m_hole, recursive, itterate_fn));
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

  FOREACH(i, hive->m_entries) {
    hive_entry_t* entry = i->data;

    if (!strcmp(entry->m_entry_part, part)) {
      return entry;
    }
  }

  return nullptr;
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
