#include "hashmap.h"
#include "dev/debug/serial.h"
#include "libk/flow/error.h"
#include "libk/string.h"
#include "logging/log.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"
#include "mem/zalloc.h"
#include <crypto/k_crc32.h>

static zone_allocator_t* __hash_entry_allocator;

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

typedef struct hashmap_entry {

  /* The actual value stored at this address */
  hashmap_value_t m_value;

  /* Hash that is computed on collision */
  uintptr_t m_hash;

  struct hashmap_entry* m_next;

} hashmap_entry_t;


/*
 * Allocate kernel memory for a hashmap entry and initialize 
 * its internals
 */
static hashmap_entry_t* __create_hashmap_entry(hashmap_value_t data, uint32_t flags, uintptr_t hash)
{
  hashmap_entry_t* entry;

  if (!data || !__hash_entry_allocator)
    return nullptr;

  entry = zalloc_fixed(__hash_entry_allocator);

  if (!entry)
    return nullptr;

  entry->m_value = data;
  entry->m_hash = hash;
  entry->m_next = nullptr;

  return entry;
}

/*
 * Destroy a hashmap_entry_t object and free its memory
 */
static void __destroy_hashmap_entry(hashmap_entry_t* entry)
{
  if (!entry || !__hash_entry_allocator)
    return;

  memset(entry, 0, sizeof(hashmap_entry_t));

  zfree_fixed(__hash_entry_allocator, entry);
}

/*
 * Compute the total size of the hashmap object based on its max size
 * and the flags
 */
static size_t __get_hashmap_size(size_t max_entries, uint32_t hm_flags)
{
  size_t hashmap_size = sizeof(hashmap_t);

  if ((hm_flags & HASHMAP_FLAG_CA) == HASHMAP_FLAG_CA) {
    hashmap_size += max_entries * sizeof(hashmap_entry_t);
  } else {
    hashmap_size += max_entries * sizeof(hashmap_value_t);
  }

  return hashmap_size;
}

/*
 * Allocate kernel memory for the hashmap and initialize 
 * its internals
 */
hashmap_t* create_hashmap(size_t max_entries, uint32_t flags) {

  hashmap_t* ret;
  size_t hashmap_size;
  size_t aligned_size;
  uint32_t delta;

  if (!max_entries)
    return nullptr;

  /* FIXME: implement open addressing */
  flags |= HASHMAP_FLAG_CA;

  hashmap_size = __get_hashmap_size(max_entries, flags);
  aligned_size = ALIGN_UP(hashmap_size, SMALL_PAGE_SIZE);

  ret = (hashmap_t*)Must(__kmem_kernel_alloc_range(aligned_size, 0, 0));

  if (!ret)
    return nullptr;

  memset(ret, 0, aligned_size);

  delta = aligned_size - hashmap_size;

  /* We are able to claim this memory just for free entries, so lets take it =D */
  if ((flags & HASHMAP_FLAG_CA) == 0)
    max_entries += (delta / sizeof(hashmap_value_t));
  else
    max_entries += (delta / sizeof(hashmap_entry_t));

  ret->m_flags = flags;
  ret->m_size = 0;
  ret->m_max_entries = max_entries;

  ret->f_hash_func = __hashmap_hash_str;

  return ret;
}

static void __hashmap_cleanup(hashmap_t* map) 
{
  hashmap_entry_t* current_entry;
  hashmap_entry_t* cached_next;

  for (uintptr_t i = 0; i < map->m_max_entries; i++) {
    current_entry = (hashmap_entry_t*)map->m_list + i;

    /* Continue if there is no next, since all the roots get cleaned up with the memset call */
    if (!current_entry->m_next)
      continue;

    current_entry = current_entry->m_next;

    /* We start at the root, which is not heap-allocated, so we skip that one */
    while (current_entry->m_next) {
      cached_next = current_entry->m_next;

      /* Destroy will invalidate the pointer */
      __destroy_hashmap_entry(current_entry);

      /* So we'll revalidate it with the cashed next */
      current_entry = cached_next;
    }
  }
}

/*
 * There SHOULD be nothing going wrong here, but you never know
 */
void destroy_hashmap(hashmap_t* map) {

  if (!map)
    return;

  __hashmap_cleanup(map);

  memset(map, 0, __get_hashmap_size(map->m_max_entries, map->m_flags));

  kfree(map);
}

ErrorOrPtr hashmap_itterate(hashmap_t* map, hashmap_itterate_fn_t fn, uint64_t arg1) {

  ErrorOrPtr result;

  if (!map || !fn)
    return Error();

  if ((map->m_flags & HASHMAP_FLAG_CA) == HASHMAP_FLAG_CA) {

    for (uint64_t i = 0; i < map->m_max_entries; i++) {
      hashmap_entry_t* entry = (hashmap_entry_t*)map->m_list + i;

      while (entry) {

        result = fn(entry->m_value, arg1);

        if (IsError(result))
          return ErrorWithVal((uint64_t)entry->m_value);
        
        entry = entry->m_next;
      }
    }

    return Success(0);
  }

  for (uint64_t i = 0; i < map->m_max_entries; i++) {
    hashmap_value_t v = map->m_list[i];

    if (!v)
      continue;
    
    result = fn(v, arg1);

    /* On error, return the value that caused it */
    if (IsError(result))
      return ErrorWithVal((uint64_t)v);
  }

  kernel_panic("TODO: implement hashmap_itterate");
  return Success(0);
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
static ErrorOrPtr __hashmap_sort_closed_entry(hashmap_entry_t* root)
{
  kernel_panic("TODO: implement __hashmap_sort_closed_entry");
  return Error();
}
 */

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

  for (uintptr_t i = 0; i < map->m_max_entries; i++) {
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
  uintptr_t idx = hashed_key % map->m_max_entries 

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

  if (map->m_size >= map->m_max_entries) {

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

/*
 * Set (mutate) a value in the map
 * NOTE: this fails if the entry does not exist
 */
ErrorOrPtr hashmap_set(hashmap_t* map, hashmap_key_t key, hashmap_value_t value) {

  if (!map || !key)
    return Error();

  if ((map->m_flags & HASHMAP_FLAG_CA) == HASHMAP_FLAG_CA)
    return __hashmap_set_closed(map, key, value);

  return __hashmap_set_open(map, key, value);
}

/*
 * Remove a value with closed addressing
 */
ErrorOrPtr __hashmap_remove_closed(hashmap_t* map, hashmap_key_t key)
{
  uintptr_t index = 0;
  hashmap_entry_t* prev;
  hashmap_entry_t* entry;

  __HASHMAP_GET_INDX(map, hashed_key, key, idx);

  entry = (hashmap_entry_t*)map->m_list + idx;

  /* No hash automatically means no entry =/ */
  if (!entry || !entry->m_hash)
    return Error();

  if (entry->m_hash == (uint32_t)hashed_key) {

    /*
     * Reset the hash and value, so the entry essentially
     * gets marked as 'free'
     */
    entry->m_hash = NULL;
    entry->m_value = NULL;

    map->m_size--;

    return Success(0);
  }

  /* No next means we don't have to continue looking xD */
  if (!entry->m_next)
    return Error();

  __HASHMAP_GET_NEW_INDX(map, new_idx, new_hash, idx, hashed_key);

  /* Cache the root */
  prev = entry;

  /* value is NULL if the entry was not found */
  entry = (hashmap_entry_t*)Release(__hashmap_find_entry(map, entry, new_hash));

  if (!entry)
    return Error();

  /* Sad implementation: linear scan to find the entry behind the one we need to remove */
  while (index++ < map->m_size) {

    if (prev->m_next == entry)
      break;

    prev = prev->m_next;
  }

  prev->m_next = entry->m_next;

  /* Clean up object */
  entry->m_value = NULL;
  entry->m_hash = NULL;
  entry->m_next = nullptr;

  map->m_size--;
  return Success(0);

}

/*
 * Remove an entry with open addressing
 */
ErrorOrPtr __hashmap_remove_open(hashmap_t* map, hashmap_key_t key)
{
  kernel_panic("TODO: implement __hashmap_remove_open");
  return Error();
}

ErrorOrPtr hashmap_remove(hashmap_t* map, hashmap_key_t key) {

  if (!map || !key)
    return Error();

  if ((map->m_flags & HASHMAP_FLAG_CA) == HASHMAP_FLAG_CA)
    return __hashmap_remove_closed(map, key);

  return __hashmap_remove_open(map, key);
}

/*!
 * @brief: Convert a hashmap into an array
 *
 * Allocates an array of the hashmaps size and tries to fill it with any valid entries it can find
 */
int hashmap_to_array(hashmap_t* map, void** array_ptr, size_t* size_ptr)
{
  uint64_t idx;
  
  if (!size_ptr || !array_ptr)
    return -1;

  idx = NULL;
  *size_ptr = sizeof(void*) * map->m_size;

  /* Currently only works for closed addressing */
  if ((map->m_flags & HASHMAP_FLAG_CA) == HASHMAP_FLAG_CA)
    return -2;

  /* Allocate an array to fit our size */
  *array_ptr = kmalloc(*size_ptr);

  for (uint64_t i = 0; i < map->m_max_entries; i++) {
    hashmap_entry_t* entry = (hashmap_entry_t*)map->m_list + i;

    while (entry) {

      /*
       * Let's assume this hashmap does not contain any valid NULL entries 
       * If there is a next entry, it does not matter, since we know we already put an
       * entry here
       */
      if (entry->m_value || entry->m_next)
        array_ptr[idx++] = entry->m_value;
      
      entry = entry->m_next;
    }
  }

  return 0;
}

void init_hashmap() 
{
  __hash_entry_allocator = create_zone_allocator_ex(nullptr, NULL, 64 * Kib, sizeof(hashmap_entry_t), 0);

  ASSERT_MSG(__hash_entry_allocator, "Failed to create hashentry allocator!");
}
