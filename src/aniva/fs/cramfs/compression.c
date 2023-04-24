#include "compression.h"
#include "dev/debug/serial.h"
#include "libk/error.h"
#include "libk/string.h"
#include "mem/heap.h"
#include "mem/kmem_manager.h"

/*
 * This file implements the DEFLATE algorithm for decompression of data.
 * it is a combination of the LZ77 algorithm and the huffman encoding.
 */

struct gzip_compressed_header {
  uint8_t compression_method;
  uint8_t flags;
  uint32_t mtime;
  uint8_t xflags;
  uint8_t os;
  uint16_t extra_bytes;
  char* name;
  char* comment;
  uint16_t crc16; /* CRC at the beginning of the compressed block */
  uint32_t crc32; /* CRC at the end of the compressed block */
};

#define MAX_NAME_LEN    128
#define MAX_COMMENT_LEN 256

struct huffman_table {
  uint16_t counts[16];
  uint16_t alphabet[288];
};

/* Lenght codes as represented by the RFC1951, section 3.2.5 */
static const uint16_t lenghts[] = {
  3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51,
  59, 67, 83, 99, 115, 131, 163, 195, 227, 258
};

/* Bitcounts for the length codes as represented by the RFC1951, section 3.2.5 */
static const uint16_t lbits[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
  4, 5, 5, 5, 5, 0
};

/* Distance codes as represented by the RFC1951, section 3.2.5 */
static const uint16_t distances[] = {
  1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
  513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
};

/* Bitcounts for the distances as represented by the RFC1951, section 3.2.5 */
static const uint16_t dbits[] = {
  0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10,
  10, 11, 11, 12, 12, 13, 13
};

/*
 * These tables contain the distances and lengths that we use to 
 * decode the huffman encoded huffman trees (Yea, mega funky), or
 * formally, the blocks which are compressed using fixed huffman coding.
 */
static struct huffman_table fixed_lengths;
static struct huffman_table fixed_distances;

static void fill_huffman_table(struct huffman_table* table, uint8_t* lengths, size_t size) {

  /* Zero */
  memset(table, 0, sizeof(struct huffman_table));

  for (size_t i = 0; i < size; i++) {
    table->counts[lengths[i]]++; 
  }

  table->counts[0] = 0;

  uint16_t offsets[16] = { 0 };
  uint32_t count = 0;

  for (uint32_t i = 0; i < 16; i++) {
    offsets[i] = count;
    count += table->counts[i];
  }

  for (size_t i = 0; i < size; i++) {
    if (lengths[i]) {
      table->alphabet[offsets[lengths[i]]] = i;
      offsets[lengths[i]]++;
    }
  }
}

static void build_fixed_tables() {

  /*
   * This is a little annoying with the local static variable,
   * but this seems like the most consistent and simple way 
   * of preventing unneeded rebuilds of the fixed tables.
   */
  static bool has_built = false;

  if (has_built) {
    return;
  }

  has_built = true;

  const uint16_t fixed_lengths_size = 288;
  const uint8_t fixed_distances_size = 30;

  uint8_t l[fixed_lengths_size];
  for (uint8_t i = 0; i < 144; i++) {
    l[i] = 8;
  }
  for (uint16_t i = 144; i < 256; i++) {
    l[i] = 9;
  }
  for (uint16_t i = 256; i < 280; i++) {
    l[i] = 7;
  }
  for (uint16_t i = 280; i < fixed_lengths_size; i++) {
    l[i] = 8;
  }

  fill_huffman_table(&fixed_lengths, l, fixed_lengths_size);

  for (uint8_t i = 0; i < fixed_distances_size; i++) {
    l[i] = 5;
  }

  fill_huffman_table(&fixed_distances, l, fixed_distances_size);

}

static void destroy_gzip_header(struct gzip_compressed_header* header) {
  kfree(header->comment);
  kfree(header->name);
}

static void c_reset_bit_buffer(decompress_ctx_t* ctx) {
  ctx->m_bit_buffer = 0;
  ctx->m_bit_buffer_bits_count = 0;
}

/*
 * When we want to read a byte, we need to reset the bit buffer in order to preserve alignment
 */
static ALWAYS_INLINE uint8_t c_read(decompress_ctx_t* ctx) {
  uint8_t ret = *ctx->m_current;
  ctx->m_current++;
  return ret;
}

/*
 * Write our uncompressed data, one byte at a time
 */
static void c_write(decompress_ctx_t* ctx, uint8_t data) {

  /* Ensure the pointer stays inbetween the bounds of our buffer */
  ctx->cache[ctx->cache_ptr & HUFFMAN_CACHE_MASK] = data;

  *ctx->m_current_out = data;

  ctx->m_current_out++;
  ctx->cache_ptr++;
}

static ALWAYS_INLINE void c_write_from(decompress_ctx_t* ctx, uint32_t offset) {
  /* Make a pointer that is always inside our buffer */
  size_t overflown_ptr = ctx->cache_ptr + HUFFMAN_CACHE_SIZE;

  ctx->cache[ctx->cache_ptr & HUFFMAN_CACHE_MASK] = ctx->cache[(overflown_ptr - offset) & HUFFMAN_CACHE_MASK];

  /* Write the current cached value into the out buffer */
  c_write(ctx, ctx->cache[ctx->cache_ptr & HUFFMAN_CACHE_MASK]);
}

static uint16_t c_read16(decompress_ctx_t* ctx) {
  uint16_t a = c_read(ctx);
  uint16_t b = c_read(ctx);
  return (b << 8) | (a << 0);
}

static uint32_t c_read32(decompress_ctx_t* ctx) {
  uint32_t a = c_read(ctx);
  uint32_t b = c_read(ctx);
  uint32_t c = c_read(ctx);
  uint32_t d = c_read(ctx);
  return (d << 24) | (c << 16) | (b << 8) | (a << 0);;
}

static uint8_t c_read_bit(decompress_ctx_t* ctx) {

  if (ctx->m_bit_buffer_bits_count == 0) {
    /* Fill the buffer */
    ctx->m_bit_buffer = c_read(ctx);
    /* We fill the buffer with 8 bits */
    ctx->m_bit_buffer_bits_count = 8;
  }

  ctx->m_bit_buffer_bits_count--;

  uint8_t ret = ctx->m_bit_buffer & 0x01;

  ctx->m_bit_buffer >>= 1;

  return ret;
}

static uint32_t c_read_bits(decompress_ctx_t* ctx, uint8_t count) {

  uint32_t ret = 0;
  for (uint8_t i = 0; i < count; i++) {
    /* Load bit into the ith place of the buffer */
    ret |= (c_read_bit(ctx) << i);
  }

  return ret;
}

static ErrorOrPtr handle_non_compressed(decompress_ctx_t* ctx, struct gzip_compressed_header* header) {

  println("Found Non-compressed block");

  /* Non-compressed blocks start with these two words */
  uint16_t len = c_read16(ctx);
  uint16_t nlen = c_read16(ctx);

  uint16_t check_len = len & 0xFFFF;
  uint16_t check_nlen = ~nlen & 0xFFFF;

  /* Check validety */
  if (check_len != check_nlen) {
    return Error();
  }

  /* Just write this data to the out buffer */
  for (uintptr_t i = 0; i < len; i++) {
    c_write(ctx, c_read(ctx));
  }

  return Success(0);
}

static ErrorOrPtr fixed_huffman_inflate(decompress_ctx_t* ctx, struct gzip_compressed_header* header) {

  // TODO: implement
  c_reset_bit_buffer(ctx);

  kernel_panic("Reached fixed huffman block");

  return Success(0);
}

static ErrorOrPtr dynamic_huffman_inflate(decompress_ctx_t* ctx, struct gzip_compressed_header* header) {

  // TODO: implement
  kernel_panic("Reached dynamic huffman block");

  return Success(0);
}

static ErrorOrPtr generic_inflate(decompress_ctx_t* ctx, struct gzip_compressed_header* header) {
  c_reset_bit_buffer(ctx);

  ErrorOrPtr result = Success(0);

  for (;;) {
    bool last = c_read_bit(ctx);
    uint8_t block_type = c_read_bits(ctx, 2);

    switch (block_type) {
      /* Not compressed */
      case 0x00:
        result = handle_non_compressed(ctx, header);
        break;
      /* fixed huffman code */
      case 0x01:
        result = fixed_huffman_inflate(ctx, header);
        break;
      /* dynamic huffman code */
      case 0x02:
        result = dynamic_huffman_inflate(ctx, header);
        break;
      /* Invalid */
      case 0x03:
      default:
        result = Error();
        break;
    }

    if (last || result.m_status != ANIVA_SUCCESS){
      break;
    }
  }

  return result;
}

#define GZIP_MAGIC_BYTE0 0x1F
#define GZIP_MAGIC_BYTE1 0x8B

#define GZIP_DEFLATE_CM 0x08

/* These flags encode to different 'optional' headers in the block */
#define GZIP_FLAG_TEXT 0x01
#define GZIP_FLAG_HCRC 0x02
#define GZIP_FLAG_EXTR 0x04
#define GZIP_FLAG_NAME 0x08
#define GZIP_FLAG_COMM 0x10

ErrorOrPtr cram_decompress(partitioned_disk_dev_t* device, void* result_buffer) {

  /* Build the fixed tables for the huffman tables if they are not built already */
  build_fixed_tables();

  decompress_ctx_t ctx = {
    .m_start_addr = device->m_partition_data.m_start_lba,
    .m_end_addr = device->m_partition_data.m_end_lba,
    .m_current = (uint8_t*)device->m_partition_data.m_start_lba,
    .m_out = result_buffer,
    .m_current_out = result_buffer,
    .cache_ptr = 0,
    .m_bit_buffer = 0,
    .m_bit_buffer_bits_count = 0,
  };

  if (c_read(&ctx) != GZIP_MAGIC_BYTE0)
    return Error();

  if (c_read(&ctx) != GZIP_MAGIC_BYTE1)
    return Error();

  struct gzip_compressed_header header = { 0 };

  /* Magic word matches, we can proceed with the decompression */

  header.compression_method = c_read(&ctx);
  header.flags = c_read(&ctx);
  header.mtime = c_read32(&ctx);
  header.xflags = c_read(&ctx);
  header.os = c_read(&ctx);

  /* Can't compress blocks with this that aren't encoded with DEFLATE */
  if (header.compression_method != GZIP_DEFLATE_CM) {
    return Error();
  }

  /* Extra bytes we can discard */
  if (header.flags & GZIP_FLAG_EXTR) {
    header.extra_bytes = c_read16(&ctx);
    for (uintptr_t i = 0; i < header.extra_bytes; i++)
      c_read(&ctx);
  }

  if (header.flags & GZIP_FLAG_NAME) {
    header.name = kmalloc(MAX_NAME_LEN);
    memset(header.name, 0, MAX_NAME_LEN);

    uintptr_t idx = 0;
    uint8_t c;
    while ((c = c_read(&ctx)) != 0x00) {
      if (idx < MAX_NAME_LEN)
        header.name[idx++] = c;
    }
  }

  println(header.name);

  if (header.flags & GZIP_FLAG_COMM) {
    header.comment = kmalloc(MAX_COMMENT_LEN);
    memset(header.comment, 0, MAX_COMMENT_LEN);

    uintptr_t idx = 0;
    uint8_t c;
    while ((c = c_read(&ctx)) != 0x00) {
      if (idx < MAX_COMMENT_LEN)
        header.comment[idx++] = c;
    }
  }

  if (header.flags & GZIP_FLAG_HCRC) {
    header.crc16 = c_read16(&ctx);
  }

  ErrorOrPtr result = generic_inflate(&ctx, &header);

  return result;
}
