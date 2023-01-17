#ifndef __ANIVA_FPU_STATE__
#define __ANIVA_FPU_STATE__
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
  uint16_t x87_ctl_wrd;
  uint16_t fsw;
  uint8_t ftw;
  uint8_t rsrv : 8;
  uint16_t FOP;
  uintptr_t FIP_64;
  uintptr_t FDP_64;

  uint32_t mxcsr;
  uint32_t mxcsr_mask;
  uint8_t mmx[128];
  uint8_t xmm[256];
  uint8_t rsrv1[96];
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

// FIXME: check current cpu capabilities for different fpu save methods
ALWAYS_INLINE void save_fpu_state(FpuState* buffer) {
  asm volatile ("fnsave %0" : "=m"(*buffer));
}

ALWAYS_INLINE void store_fpu_state(FpuState* buffer) {
  asm volatile ("frstor %0" :: "m"(*buffer));
}

#endif // !__ANIVA_FPU_STATE__
