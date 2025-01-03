.section .text

.global _start
.extern _start
.type _start, @function

.extern lightapp_startup

/*
 * Epic entrypoint for our userspace apps
 */
_start:
    andq $0xFFFFFFFFFFFFFFF0, %rsp

    // Instantly move over to the C environment
    // while preserving rdi
    call lightapp_startup

.size _start, . - _start
