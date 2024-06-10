#ifndef __ANIVA_KEVENT_KERROR__
#define __ANIVA_KEVENT_KERROR__

#include "libk/stddef.h"
#include <system/processor/registers.h>

struct kevent_ctx;

enum KERROR_TYPE {
    KERROR_PF = 0,
    KERROR_GPF,
    KERROR_DIVIDE_BY_ZERO,
    KERROR_NMI,
    KERROR_ILLEGAL_INSTRUCTION,
    KERROR_FPU_ERROR,
    KERROR_DOUBLE_FAULT,
    KERROR_SEGMENT_OVERRUN,
    KERROR_INVALID_TSS,
    KERROR_NO_SEGMENT,
    KERROR_STACK_SEGMENT_FAULT,
    KERROR_X87_FAULT,
    KERROR_ALIGNMENT_FAULT,
    KERROR_SIMD_FP_FAULT,
    KERROR_VIRTUALIZATION_FAULT,
    KERROR_CTL_PROT_FAULT,
    KERROR_HYPERVISOR_INJ_FAULT,
    KERROR_VMM_COMM_FAULT,
    KERROR_SECURITY_FAULT,

    KERROR_ASSERT_FAILED,
    KERROR_UNKNOWN,
};

/* Follow unix convention where negative errorcodes mark fatal errors */
#define ERR_FATAL 0x8000000000000000ULL

/*
 * Centralized kernel errors
 *
 * This is where most, if not all kernel errors should be directed to and handled.
 *
 */
typedef struct kevent_error_ctx {
    enum KERROR_TYPE type;
    int code;
    registers_t* cpu_state;
} kevent_error_ctx_t;

void throw_kerror(registers_t* cpu_state, enum KERROR_TYPE type, int code);

extern int NORETURN __default_kerror_handler(struct kevent_ctx* ctx, void* param);
#endif // !__ANIVA_KEVENT_KERROR__
