#include "init.h"
#include "system/asm_specifics.h"
#include "system/processor/processor_info.h"
#include <libk/stddef.h>
#include <system/processor/processor.h>

void fpu_generic_init()
{
    // FIXME: for now we assume that this only gets called on startup,
    // so current == bsp, but this won't be the case later on...
    processor_t* current = get_current_processor();
    unsigned long cr0, cr4_msk = 0;

    if (processor_has(&current->m_info, X86_FEATURE_FXSR)) {
        cr4_msk |= (1 << 9);
    }

    if (cr4_msk) {
        write_cr4(read_cr4() | cr4_msk);
    }

    cr0 = read_cr0();
    cr0 &= ~((1 << 2) | (1 << 3));
    if (!processor_has(&current->m_info, X86_FEATURE_FPU)) {
        cr0 |= (1 << 2);
    }

    write_cr0(cr0);

    asm volatile("fninit");
}
