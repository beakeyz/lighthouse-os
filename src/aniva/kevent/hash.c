#include "hash.h"
#include <libk/string.h>


static inline uint32_t ke_keygen_get_seed()
{
  /* TODO: randomize this seed */
  return 4498582;
}

/*
 * MurMur Hash scramble function
 */
static inline uint32_t ke_keygen_scramble(uint32_t key)
{
  key *= 0xcc9e2d51;
  key = (key << 15) | (key >> 17);
  key *= 0x1b873593;
  return key;
}

/*!
 * @brief: MurMur Hash function
 *
 * Used to compute a hash for a kevent hook
 * NOTE: this is for little-endian CPUs only
 */
uint32_t kevent_compute_hook_key(const char* hook_name)
{
  size_t len = strlen(hook_name);
  uint32_t h = ke_keygen_get_seed();
  uint32_t k = 0;

  /* Read in groups of 4. */
  for (size_t i = len >> 2; i; i--) {
      memcpy(&k, hook_name, sizeof(uint32_t));
      hook_name += sizeof(uint32_t);
      h ^= ke_keygen_scramble(k);
      h = (h << 13) | (h >> 19);
      h = h * 5 + 0xe6546b64;
  }

  /* Read the rest. */
  k = 0;
  for (size_t i = len & 3; i; i--) {
      k <<= 8;
      k |= hook_name[i - 1];
  }

  h ^= ke_keygen_scramble(k);
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}
