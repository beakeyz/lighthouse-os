
#include "memory.h"
#include "LibSys/syscall.h"
#include <LibSys/system.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define ALIGN_UP(addr, size) \
    (((addr) % (size) == 0) ? (addr) : (addr) + (size) - ((addr) % (size)))

#define ALIGN_DOWN(addr, size) ((addr) - ((addr) % (size)))

#define DEFAULT_INITIAL_RANGE_SIZE (80 * Kib)

static uint32_t default_range_entry_sizes[] = {
  [0] = 8,
  [1] = 16,
  [2] = 24,
  [3] = 32,
  [4] = 64,
  [5] = 88,
  [6] = 128,
  [7] = 188,
  [8] = 256,
  [9] = 512,
  [10] = 1024,
  [11] = 2048,
  [12] = 4096,
  [13] = 8192,
  [14] = 16384,
  [15] = 40960,
};

const static uint32_t default_entry_sizes = sizeof default_range_entry_sizes / sizeof default_range_entry_sizes[0];

/*
 * A range of malloc buckets
 */
struct malloc_range {
  struct malloc_range* m_next_size;
  struct malloc_range* m_next;
  uint32_t m_data_size;
  uint32_t m_entry_size;
  uint32_t m_data_start_offset;
  uint8_t m_bytes[];
};

/*
 * The bitmap is used for keeping track of the allocations per malloc range
 */
struct malloc_bitmap {
  uint32_t entry_count;
  uint16_t m_words[];
};

static struct malloc_range* __start_range;

static struct malloc_range* __malloc_get_privious_size(uint32_t entry_size)
{
  struct malloc_range* ittr;
  struct malloc_range* prev;

  ittr = __start_range;
  prev = nullptr;

  while (ittr) {

    /* This size already exists: just stick it below it */
    if (ittr->m_entry_size == entry_size)
      return ittr;

    /* We reached the end: stick it at tbitmap->entry_count << ohe end */
    if (ittr->m_entry_size < entry_size && !ittr->m_next_size)
      return ittr;

    /* We've just passed it: return the previous one */
    if (ittr->m_entry_size > entry_size)
      return prev;

    prev = ittr;
    ittr = ittr->m_next_size;
  }

  return nullptr;
}

/*
 * Make a bitmap that optimizes the amount of entries we can hold
 */
static void __malloc_init_bitmap(struct malloc_range* range)
{
  uint32_t prev_bitmap_size;
  uint32_t start_data_size;
  uint32_t entry_count;
  uint32_t bitmap_size;
  struct malloc_bitmap* bitmap;

  entry_count = NULL;
  bitmap_size = NULL;
  start_data_size = range->m_data_size;
  bitmap = (struct malloc_bitmap*)&range->m_bytes[0];

  do {
    entry_count = ALIGN_DOWN(range->m_data_size, range->m_entry_size) / range->m_entry_size;

    prev_bitmap_size = bitmap_size;
    bitmap_size = (entry_count >> 4);

    range->m_data_size = start_data_size - bitmap_size;

  } while (prev_bitmap_size != bitmap_size);

  bitmap->entry_count = entry_count;

  memset(&bitmap->m_words, 0, bitmap_size);
}

static inline struct malloc_bitmap* malloc_get_bitmap(struct malloc_range* range) 
{
  return (struct malloc_bitmap*)(&range->m_bytes[0]);
}

static inline uint8_t* malloc_range_get_start(struct malloc_range* range)
{
  return (&range->m_bytes[range->m_data_start_offset]);
}

/*
 * Add a range of a pagealigned size
 */
static void __add_malloc_size_range(uint32_t total_size, uint32_t entry_size_idx)
{
  void* buffer;
  uint32_t data_size;
  uint32_t bitmap_size;
  struct malloc_range* previous;
  struct malloc_range* range;
  syscall_result_t result;

  if (!total_size)
    return;

  result = syscall_3(SYSID_ALLOCATE_PAGES, total_size, NULL, (uint64_t)&buffer);

  if (result != SYS_OK)
    return;

  range = (struct malloc_range*)buffer;

  memset(range, 0, sizeof(struct malloc_range));

  /* Point this range to the previous first range */
  range->m_data_size = total_size - sizeof(struct malloc_range) - sizeof(struct malloc_bitmap);
  range->m_entry_size = default_range_entry_sizes[entry_size_idx];

  __malloc_init_bitmap(range);

  range->m_data_start_offset = ALIGN_UP((sizeof(struct malloc_bitmap) + (malloc_get_bitmap(range)->entry_count >> 4)), range->m_entry_size);

  previous = __malloc_get_privious_size(range->m_entry_size);

  if (!previous) {
    __start_range = range;
    return;
  }

  range->m_next_size = previous->m_next_size;

  if (previous->m_entry_size == range->m_entry_size) {
    range->m_next = previous->m_next;
    previous->m_next = range;
  } else {
    previous->m_next_size = range;
  }
}

/*!
 * @brief Check if a malloc range contains an address
 *
 * Nothing to add here...
 */
static bool malloc_range_contains(struct malloc_range* range, uint64_t addr)
{
  uint64_t start = (uint64_t)malloc_range_get_start(range);
  return (addr >= start && addr < start + range->m_data_size);
}

/*!
 * @brief Find the range that contains an address
 *
 * Nothing to add here...
 */
static struct malloc_range* malloc_find_containing_range(uint64_t addr)
{
  struct malloc_range* current;

  current = __start_range;

  while (current) {

    if (malloc_range_contains(current, addr))
      return current;

    current = current->m_next;

    if (!current)
      current = current->m_next_size;
  }

  return nullptr;
}

static struct malloc_range* malloc_find_range_for_size(size_t size)
{
  struct malloc_range* current;

  if (!size || size > default_range_entry_sizes[default_entry_sizes-1])
    return nullptr;

  current = __start_range;

  do {

    /* Just use the first range that fits the size */
    if (size <= current->m_entry_size)
      return current;

    current = current->m_next_size;
  } while(current);

  return nullptr;
}

static bool malloc_find_free_offset(struct malloc_range* range, uint32_t* result)
{
  uint16_t entry;
  struct malloc_bitmap* bitmap;

  if (!result)
    return false;

  bitmap = malloc_get_bitmap(range);

  for (uint32_t i = 0; i < bitmap->entry_count; i++) {
    entry = bitmap->m_words[i];

    if (entry == 0xffff)
      continue;

    for (uint8_t j = 0; j < 16; j++) {
      if (!(entry & (1 << j)))
        continue;

      *result = (i * 16) + j;
      return true;
    }
  }

  return false;
}

static void malloc_range_set_free(struct malloc_range* range, uint32_t offset)
{
  struct malloc_bitmap* bitmap;

  bitmap = malloc_get_bitmap(range);

  uint32_t index_word = offset >> 4;
  uint32_t index_bit = offset % 16;

  bitmap->m_words[index_word] &= ~(1 << index_bit);
}

static void malloc_range_set_used(struct malloc_range* range, uint32_t offset)
{
  struct malloc_bitmap* bitmap;

  bitmap = malloc_get_bitmap(range);

  uint32_t index_word = offset >> 4;
  uint32_t index_bit = offset % 16;

  bitmap->m_words[index_word] |= (1 << index_bit);
}

/*
 * Until we have dynamic loading of libraries,
 * we want to keep libraries using libraries to a
 * minimum. This is because when we statically link 
 * everything together and for example we are using 
 * LibSys here, it gets included in the binary, which itself 
 * does not use any other functionallity of LibSys, so thats 
 * kinda lame imo
 *
 * TODO: check if the process has requested a heap of a specific size
 */
void __init_memalloc(void)
{
  /* Initialize structures */
  /* Ask for a memory region from the kernel */
  /* Use the initial memory we get to initialize the allocator further */
  /* Mark readiness */

  __start_range = NULL;

  for (uint32_t i = 0; i < default_entry_sizes; i++) {
    __add_malloc_size_range(DEFAULT_INITIAL_RANGE_SIZE, i);
  }

  exit(0);
}


void* mem_alloc(size_t size, uint32_t flags)
{
  bool result;
  uint32_t offset;
  struct malloc_range* range;

  range = malloc_find_range_for_size(size);
  
  if (!range)
    return nullptr;

  do {
    result = malloc_find_free_offset(range, &offset);

    if (result) {
      malloc_range_set_used(range, offset);

      return (malloc_range_get_start(range) + offset);
    }

    range = range->m_next;
  } while(range);

  /* TODO: add a new range and try again */
  return nullptr;
}

void* mem_move_alloc(void* ptr, size_t new_size, uint32_t flags) 
{
  /* Also nope =) */
  return nullptr;
}

int mem_dealloc(void* addr, uint32_t flags)
{
  /* No */
  return 0;
}
