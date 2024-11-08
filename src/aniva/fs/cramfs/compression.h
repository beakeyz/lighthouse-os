#ifndef __ANIVA_CRAMFS_DECOMPRESSION__
#define __ANIVA_CRAMFS_DECOMPRESSION__

#include "dev/disk/volume.h"
#include "libk/flow/error.h"
#include <libk/stddef.h>

#define HUFFMAN_CACHE_SIZE 0x8000
#define HUFFMAN_CACHE_MASK 0x7FFF

struct huffman_cache {
    /*
     * The RFC 1951 reference specifies a maximum distance of 32K bytes. This
     * is subsequently how far we need to be able to 'look back' into our stuff.
     */
    uintptr_t cache_ptr;
    uint8_t cache[HUFFMAN_CACHE_SIZE];
};

typedef struct {
    uintptr_t m_start_addr;
    uintptr_t m_end_addr;
    uint8_t* m_current; /* Itterator pointer in the compressed data */
    uint8_t* m_out; /* Buffer to our uncompressed data */
    uint8_t* m_current_out; /* Itterator in our uncompressed buffer */
    /* Buffers we will use to read bits */
    uint32_t m_bit_buffer;
    uint32_t m_bit_buffer_bits_count;

    struct huffman_cache* m_cache;
} decompress_ctx_t;

kerror_t cram_decompress(volume_t* device, void* result_buffer);

bool cram_is_compressed_library(volume_t* device);

size_t cram_find_decompressed_size(volume_t* device);

#endif // !__ANIVA_CRAMFS_DECOMPRESSION__
