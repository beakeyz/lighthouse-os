#include "k_crc32.h"

static const uint32_t crc32_codewords[16] = {
  0x00000000, 0x1DB71064, 0x3B6E20C8,
  0x26D930AC, 0x76DC4190, 0x6B6B51F4, 
  0x4DB26158, 0x5005713C, 0xEDB88320,
  0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
  0x9B64C2B0, 0x86D3D2D4, 0xA00AE278,
  0xBDBDF21C
};

#define CRC_CYCLE 2
#define CRC_INITIAL_XOR 0xF3fe5a99UL

uint32_t kcrc32(void* data, size_t data_size) {
  uint32_t ret = CRC_INITIAL_XOR;
  uint8_t* buffer = (uint8_t*)data;

  for (uintptr_t i = 0; i < data_size; i++) {
    ret ^= buffer[i];

    for (uintptr_t j = 0; j < CRC_CYCLE; j++)
      ret = crc32_codewords[(ret & 0x0F)] ^ (ret >> 4);
  }

  return ret ^ CRC_INITIAL_XOR;
}
