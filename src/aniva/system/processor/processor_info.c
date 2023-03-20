#include "processor_info.h"
#include "system/asm_specifics.h"
#include "libk/string.h"
#include "dev/debug/serial.h"

#define GET_PROCESSOR_MODEL(eax) ((((eax) >> 4) & 0xf) + ((((eax) >> 16) & 0xf) << 4))
#define GET_PROCESSOR_FAMILY(eax) ((((eax) >> 8) & 0xf) + ((((eax) >> 20) & 0xff)))

static void get_processor_model_name(processor_info_t* info);
static void detect_processor_cores(processor_info_t* info);
static void detect_processor_capabilities(processor_info_t* info);
void detect_processor_cache_sizes(processor_info_t* info);
static void resolve_processor_cap_implications(processor_info_t* info);

processor_info_t gather_processor_info() {
  processor_info_t info = {0};

  memset(&info.m_x86_capabilities, 0x00, sizeof(info.m_x86_capabilities));

  uint32_t cpuid_eax;
  uint32_t cpuid_ebx;
  uint32_t cpuid_ecx;
  uint32_t cpuid_edx;
  read_cpuid(0x80000000, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

  info.m_cpuid_level_extended = cpuid_eax;

  read_cpuid(0x00000000, 0,
        (unsigned int *)&info.m_cpuid_level,
        (unsigned int *)&info.m_vendor_id[0],
        (unsigned int *)&info.m_vendor_id[8],
        (unsigned int *)&info.m_vendor_id[4]);

  if (info.m_cpuid_level >= 0x00000001) {
    read_cpuid(0x00000001, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

    info.m_family = GET_PROCESSOR_FAMILY(cpuid_eax);
    info.m_model = GET_PROCESSOR_MODEL(cpuid_eax);
    info.m_stepping = cpuid_eax & 0xf;

    if (cpuid_edx & (1 << 19)) {
      info.m_cache_alignment = ((cpuid_ebx >> 8) & 0xff) * 8;
    }
  }

  if (info.m_cpuid_level_extended >= 0x80000008) {
    read_cpuid(0x80000008, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

    info.m_virtual_bit_width = (cpuid_eax >> 8) & 0xff;
    info.m_physical_bit_width = cpuid_eax & 0xff;
  }

  detect_processor_capabilities(&info);

  detect_processor_cores(&info);
  detect_processor_cache_sizes(&info);
  get_processor_model_name(&info);

  return info;
}

void detect_processor_cache_sizes(processor_info_t* info) {
  uint32_t ext_lvl, cpuid_eax, cpuid_ebx, cpuid_ecx, cpuid_edx;

  ext_lvl = info->m_cpuid_level_extended;

  if (ext_lvl >= 0x80000005) {
    read_cpuid(0x80000005, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

    info->m_l1_data.m_cache_size = ((cpuid_ecx >> 24) & 0xff) * Kib;
    info->m_l1_data.m_cache_line_size = cpuid_ecx & 0xff;
    info->m_l1_data.m_has_cache = true;
    info->m_tlb_size = 0;

    info->m_l1_instruction.m_cache_size = ((cpuid_edx >> 24) & 0xff) * Kib;
    info->m_l1_instruction.m_cache_line_size = cpuid_edx & 0xff;
    info->m_l1_instruction.m_has_cache = true;
  }

  /*
   * no other cache we can probe
   */
  if (ext_lvl < 0x80000006)
    return;

  read_cpuid(0x80000006, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

  if (cpuid_ecx) {
    info->m_l2.m_cache_size = ((cpuid_ecx >> 16) & 0xffff) * Kib;
    info->m_l2.m_cache_line_size = cpuid_ecx & 0xff;
    info->m_l2.m_has_cache = true;
  }

  if (cpuid_edx) {
    info->m_l3.m_cache_size = ((cpuid_ecx >> 18) & 0x3fff) * 512 * Kib;
    info->m_l3.m_cache_line_size = cpuid_ecx & 0xff;
    info->m_l3.m_has_cache = true;
  }
}

/*
 * detect the capabilities our processor has and store them in the
 * uint32_t array we have in the processor_t structure
 */
static void detect_processor_capabilities(processor_info_t* info) {
  uint32_t cpuid_eax, cpuid_ebx, cpuid_ecx, cpuid_edx;

  uint32_t cpuid_level = info->m_cpuid_level;

  if (cpuid_level >= 0x00000001) {
    read_cpuid(0x00000001, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

    info->m_x86_capabilities[CPUID_1_ECX] = cpuid_ecx;
    info->m_x86_capabilities[CPUID_1_EDX] = cpuid_edx;
  }

  if (cpuid_level >= 0x00000006) {
    read_cpuid(0x00000006, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

    info->m_x86_capabilities[CPUID_6_EAX] = cpuid_eax;
  }

  if (cpuid_level >= 0x00000007) {
    read_cpuid(0x00000007, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

    info->m_x86_capabilities[CPUID_7_ECX] = cpuid_ecx;
    info->m_x86_capabilities[CPUID_7_EDX] = cpuid_edx;
    info->m_x86_capabilities[CPUID_7_0_EBX] = cpuid_ebx;

    if (cpuid_eax > 1) {
      read_cpuid(0x00000007, 1, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
      info->m_x86_capabilities[CPUID_7_1_EAX] = cpuid_eax;
    }
  }

  if (cpuid_level >= 0x0000000d) {
    read_cpuid(0x0000000d, 1, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
    info->m_x86_capabilities[CPUID_D_1_EAX] = cpuid_eax;
  }

  read_cpuid(0x80000000, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
  info->m_cpuid_level_extended = cpuid_eax;

  if ((cpuid_eax & 0xffff0000) == 0x80000000 && cpuid_eax >= 0x80000001) {
    read_cpuid(0x80000001, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);

    info->m_x86_capabilities[CPUID_8000_0001_ECX] = cpuid_ecx;
    info->m_x86_capabilities[CPUID_8000_0001_EDX] = cpuid_edx;
  }

  if (info->m_cpuid_level_extended >= 0x80000007) {
    read_cpuid(0x80000007, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
    info->m_x86_capabilities[CPUID_8000_0007_EBX] = cpuid_ebx;
  }

  if (info->m_cpuid_level_extended >= 0x80000008) {
    read_cpuid(0x80000008, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
    info->m_x86_capabilities[CPUID_8000_0008_EBX] = cpuid_ebx;
  }
  if (info->m_cpuid_level_extended >= 0x8000000a) {
    read_cpuid(0x8000000a, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
    info->m_x86_capabilities[CPUID_8000_000A_EDX] = cpuid_edx;
  }
  if (info->m_cpuid_level_extended >= 0x8000001f) {
    read_cpuid(0x8000001f, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
    info->m_x86_capabilities[CPUID_8000_001F_EAX] = cpuid_eax;
  }


}

/*
 * some features imply the support for other features.
 * we need to make sure these are all set accordingly
 */
static void resolve_processor_cap_implications(processor_info_t* info) {

}

static void get_processor_model_name(processor_info_t* info) {
  uint32_t *buffer;

  buffer = (uint32_t*)info->m_model_id;
  read_cpuid(0x80000002, 0, &buffer[0], &buffer[1], &buffer[2], &buffer[3]);
  read_cpuid(0x80000003, 0, &buffer[4], &buffer[5], &buffer[6], &buffer[7]);
  read_cpuid(0x80000004, 0, &buffer[8], &buffer[9], &buffer[10], &buffer[11]);
  info->m_model_id[48] = 0;

  // TODO: finalize this string, since it might still have whitespace and stuff
}

static void detect_processor_cores(processor_info_t* info) {

  info->m_max_available_cores = 1;
  if (info->m_cpuid_level < 4) {
    return;
  }

  uint32_t eax, ebx, ecx, edx;

  read_cpuid(4, 0, &eax, &ebx, &ecx, &edx);

  if (eax & 0x1f)
    info->m_max_available_cores = (eax >> 26) + 1;
}
