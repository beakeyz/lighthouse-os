[global __memcpy]



    ; Copies a memory block from dest to src
    ;
    ; %rdi -> dest
    ; %rsi -> src
    ; %rdx -> buffersize
    ; %rax <- original src location
    ;
__memcpy:
    ret
