#include "hashmap.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/string.h"
#include "mem/heap.h"
#include <crypto/k_crc32.h>

/*
 * Main hash function for string keys
 * FIXME: should we make this accept a dynamic memory size, in order to merge 
 *        string keys and non-string keys to be used?
 */
static uintptr_t __hashmap_hash_str(hashmap_key_t str)
{
  char* _str = (char*)str;

  uint32_t hashed_key = kcrc32(_str, strlen(_str));

  return hashed_key;
}

/*
 * Allocate kernel memory for a hashmap entry and initialize 
 * its internals
 */
static hashmap_entry_t* __create_hashmap_entry(hashmap_value_t data, uint32_t flags, uintptr_t hash)
{
  hashmap_entry_t* entry;

  if (!data)
    return nullptr;

  entry = kmalloc(sizeof(hashmap_entry_t));

  if (!entry)
    return nullptr;

  entry->m_value = data;
  entry->m_hash = hash;
  entry->m_next = nullptr;

  return entry;
}

/*
 * Allocate kernel memory for the hashmap and initialize 
 * its internals
 */
hashmap_t* create_hashmap(size_t max_size, uint32_t flags) {

  hashmap_t* ret;
  size_t hashmap_size;

  if (!max_size)
    return nullptr;

  hashmap_size = sizeof(hashmap_t) + (max_size * sizeof(hashmap_entry_t));

  ret = kmalloc(hashmap_size);

  if (!ret)
    return nullptr;

  memset(ret, 0, hashmap_size);

  /* FIXME: implement open addressing */
  flags |= HASHMAP_FLAG_CA;

  ret->m_flags = flags;
  ret->m_size = 0;
  ret->m_max_size = max_size;

  ret->f_hash_func = __hashmap_hash_str;

  return ret;
}

/*
 * TODO
 * Attempt a resize of the hashmap this means:
 * 1) Find a region of memory large enough to fit the new map
 * 2) copy over the entries of the old map to the new map, 
 *    in such a way that the hashing indecies don't get fucked
 * 3) Fixup pointers (?)
 */
static ErrorOrPtr __hashmap_try_resize(hashmap_t* map)
{
  kernel_panic("TODO: implement __hashmap_try_resize");
  return Error();
}

/*
 * Sort the closed entry buffer based on hash from low to high
 */
static ErrorOrPtr __hashmap_sort_closed_entry(hashmap_entry_t* root)
{
  kernel_panic("TODO: implement __hashmap_sort_closed_entry");
  return Error();
}

/*
 * Find the middle pointer in the linked list of entries
 * end is nullable
 */
static ErrorOrPtr __hashmap_find_middle_entry(hashmap_entry_t* root, hashmap_entry_t* end)
{
  hashmap_entry_t* slow;
  hashmap_entry_t* fast;

  if (!root)
    return Error();

  slow = root;
  fast = root;

  /*
   * Directly plug end into the check, since NULL in this case 
   * means the end of the chain and any other value should just
   * be regarded as the end of the list
   */
  while (fast && fast->m_next && fast != end && fast->m_next != end) {
    slow = slow->m_next;
    fast = fast->m_next->m_next;
  }

  return Success((uintptr_t)slow);
}

/*
 * Find a hashmap entry though the key, by using binary search which 
 * works on the fact that entries are sorted based on hash (low -> high)
 */
static ErrorOrPtr __hashmap_find_entry(hashmap_t* map, hashmap_entry_t* root, uintptr_t key)
{

  hashmap_entry_t* current_end;
  hashmap_entry_t* current_start;

  if (!map || !root)
    return Error();

  current_end = nullptr;
  current_start = root;

  for (uintptr_t i = 0; i < map->m_max_size; i++) {
    hashmap_entry_t* middle = (hashmap_entry_t*)Release(__hashmap_find_middle_entry(current_start, current_end));

    if (!middle)
      return Error();

    /* Bigger, the desired key is in the next few entries */
    if (key > middle->m_hash) {
      current_start = middle;
      continue;
    }

    /* Smaller, the desired key is in the previous few entries */
    if (key < middle->m_hash) {
      current_end = middle;
      continue;
    }

    /* Equal, we have found our entry */
    return Success((uintptr_t)middle);
  }

  return Error();
}

#define __HASHMAP_GET_INDX(map, hashed_key, key, idx)                   \
  uintptr_t hashed_key = map->f_hash_func(key);                         \
  uintptr_t idx = hashed_key % map->m_max_size 

/* NOTE: the new hash should always be more than the hash of the root node */
#define __HASHMAP_GET_NEW_INDX(map, new_idx, new_hash, idx, hashed_key) \
  uintptr_t new_idx = (idx + hashed_key) << idx;                                 \
  uintptr_t new_hash = (hashed_key + (uintptr_t)kcrc32(&new_idx, sizeof(uintptr_t)))


/*
 * Procedure for duplicate hits:
 * when a double hit occurs, we rehash the original index with the hashed_key as a salt.
 * then, we insert the entry in order of that hash from low to high.
 */
static ErrorOrPtr __hashmap_put_closed(hashmap_t* map, hashmap_key_t key, hashmap_value_t value)
{

  __HASHMAP_GET_INDX(map, hashed_key, key, idx);

  hashmap_entry_t* entry = (hashmap_entry_t*)map->m_list + idx;

  /* Free entry. Just put it there */
  if (!entry->m_value && !entry->m_hash) {
    entry->m_value = value;
    entry->m_next = nullptr;
    entry->m_hash = hashed_key;

    map->m_size++;
    return Success(0);
  }

  /* Duplicate entry, fuck off */
  if (entry->m_hash == hashed_key) {
    return Error();
  }

  __HASHMAP_GET_NEW_INDX(map, new_idx, new_hash, idx, hashed_key);

  hashmap_entry_t* prev;
  hashmap_entry_t* new_entry = __create_hashmap_entry(value, NULL, new_hash);

  if (!new_entry)
    return Error();

  while (entry) {

    prev = entry;
    entry = entry->m_next;

    uintptr_t prev_hash = 0;
    uintptr_t next_hash = (uintptr_t)-1;

    if (entry)
      next_hash = entry->m_hash;

    prev_hash = prev->m_hash;

    if (new_hash >= prev_hash && new_hash <= next_hash)
      break;

  }

  /* NOTE: prev may not be NULL, but entry may */
  if (!prev)
    return Error();

  prev->m_next = new_entry;
  new_entry->m_next = entry;

  map->m_size++;

  return Success(0);
}

/*
 * Try to put a value into the hashmap with open addressing
 */
static ErrorOrPtr __hashmap_put_open(hashmap_t* map, hashmap_key_t key, hashmap_value_t value)
{
  kernel_panic("TODO: implement __hashmap_put_open");
  return Error();
}

/*
 * Try to put a value into the hashmap based on the key 
 * and type of hashmap addressing. When there is no room left
 * in the hashmap, we try to resize (reallocate) it.
 *
 * NOTE: when it seems like the entries in closed addressing are 
 * unsorted, we attempt to sort the entries. When this fails, that means that the 
 * hashmap is partially unusable at this point, so we'll have to either switch
 * to linear scanning of the linked lists or we invalidate the entire hashmap and crash =/
 */
ErrorOrPtr hashmap_put(hashmap_t* map, hashmap_key_t key, hashmap_value_t value) {

  /* This implies we don't allow nullable values. KEEP THIS CONSISTENT */
  if (!map || !key || !value)
    return Error();

  if (map->m_size >= map->m_max_size) {

    if (map->m_flags & HASHMAP_FLAG_FS)
      return Error();
    
    TRY(_, __hashmap_try_resize(map));
  }

  if ((map->m_flags & HASHMAP_FLAG_CA) == HASHMAP_FLAG_CA)
    return __hashmap_put_closed(map, key, value);

  return __hashmap_put_open(map, key, value);
}

/*
 * Try to get a value from the hashmap with closed addressing
 */
static hashmap_value_t __hashmap_get_closed(hashmap_t* map, hashmap_key_t key) 
{
  hashmap_entry_t* entry;

  __HASHMAP_GET_INDX(map, hashed_key, key, idx);

  entry = (hashmap_entry_t*)map->m_list + idx;

  /* No hash automatically means no entry =/ */
  if (!entry || !entry->m_hash)
    return nullptr;

  if (entry->m_hash == (uint32_t)hashed_key) {
    return entry->m_value;
  }

  /* No next means we don't have to continue looking xD */
  if (!entry->m_next)
    return nullptr;

  __HASHMAP_GET_NEW_INDX(map, new_idx, new_hash, idx, hashed_key);

  /* value is NULL if the entry was not found */
  entry = (hashmap_entry_t*)Release(__hashmap_find_entry(map, entry, new_hash));

  if (!entry)
    return nullptr;

  ASSERT_MSG(entry->m_hash == new_hash, "Sanity check failed: (hashmap:__hashmap_get_closed) ->m_hash != new_hash!");

  return entry->m_value;
}

/*
 * Try to get a value from the hashmap with open addressing
 */
static hashmap_value_t __hashmap_get_open(hashmap_t* map, hashmap_key_t key) 
{
  kernel_panic("TODO: implement __hashmap_get_open");
}

/*
 * Try to get a value from the hashmap based on the key 
 * and type of hashmap addressing
 */
hashmap_value_t hashmap_get(hashmap_t* map, hashmap_key_t key) {

  if (!map || !key)
    return nullptr;

  if ((map->m_flags & HASHMAP_FLAG_CA) == HASHMAP_FLAG_CA)
    return __hashmap_get_closed(map, key);

  return __hashmap_get_open(map, key);
}

/*
 * Find an entry in the hashmap and set its value
 *
 * __hashmap_get_closed and __hashmap_set_closed are very similare, so we need
 * to find ways to make em share some more code (FIXME)
 */
static ErrorOrPtr __hashmap_set_closed(hashmap_t* map, hashmap_key_t key, hashmap_value_t value) 
{
  uintptr_t old_value;
  hashmap_entry_t* entry;

  __HASHMAP_GET_INDX(map, hashed_key, key, idx);

  entry = (hashmap_entry_t*)map->m_list + idx;

  /* No hash automatically means no entry =/ */
  if (!entry || !entry->m_hash)
    return Error();

  if (entry->m_hash == (uint32_t)hashed_key) {
    old_value = (uintptr_t)entry->m_value;
    entry->m_value = value;
    return Success(old_value);
  }

  /* No next means we don't have to continue looking xD */
  if (!entry->m_next)
    return Error();

  __HASHMAP_GET_NEW_INDX(map, new_idx, new_hash, idx, hashed_key);


  /* value is NULL if the entry was not found */
  entry = (hashmap_entry_t*)Release(__hashmap_find_entry(map, entry, new_hash));

  if (!entry)
    return Error();

  ASSERT_MSG(entry->m_hash == new_hash, "Sanity check failed: (hashmap:__hashmap_get_closed) ->m_hash != new_hash!");

  old_value = (uintptr_t)entry->m_value;
  entry->m_value = value;

  return Success(old_value);
}

static ErrorOrPtr __hashmap_set_open(hashmap_t* map, hashmap_key_t key, hashmap_value_t value) 
{
  kernel_panic("TODO: implement __hashmap_set_open");
}

ErrorOrPtr hashmap_set(hashmap_t* map, hashmap_key_t key, hashmap_value_t value) {

  if (!map || !key)
    return Error();

  if ((map->m_flags & HASHMAP_FLAG_CA) == HASHMAP_FLAG_CA)
    return __hashmap_set_closed(map, key, value);

  return __hashmap_set_open(map, key, value);
}
