#ifndef __LIGHT_FPU_STATE__
#define __LIGHT_FPU_STATE__
#include <libk/stddef.h>

typedef enum state_comp {
  X87 = 1ull << 0ull,
  SSE = 1ull << 1ull, 
  AVX = 1ull << 2ull, 
  MPX_BNDREGS = 1ull << 3ull,
  MPX_BNDCSR = 1ull << 4ull,
  AVX512_opmask = 1ull << 5ull, 
  AVX512_ZMM_hi = 1ull << 6ull, 
  AVX512_ZMM = 1ull << 7ull,    
  PT = 1ull << 8ull,
  PKRU = 1ull << 9ull,
  CET_U = 1ull << 11ull,
  CET_S = 1ull << 12ull,
  HDC = 1ull << 13ull,
  LBR = 1ull << 15ull,
  HWP = 1ull << 16ull,
  XCOMP_ENABLE = 1ull << 63ull
} state_comp_t;

typedef struct {
  // TODO
} __attribute__((packed)) legacy_region_t;

typedef struct {
  state_comp_t xstate;
  state_comp_t xcomp;
  uint8_t rsrv[48]; 
} __attribute__((packed)) state_header_t;

typedef struct {
  // lr
  legacy_region_t region;
  // header
  state_header_t header;
  // ext save area (CPUID)
  uint8_t esa[256]; 
} __attribute__((aligned(64), packed)) FpuState;

#endif // !__LIGHT_FPU_STATE__
