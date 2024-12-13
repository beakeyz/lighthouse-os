[section .text]

[global ___app_trampoline]

[global __app_entrypoint]
[global __lib_entrypoints]
[global __lib_entrycount]
[global __exit_vec]

SYSID_GET_EXITVEC equ 2

;
; App entry trampoline
; 
; This stub is a bit of code that gets executed in userspace, after it's been
; relocated by the dynamic loader. This bit of code gets copied/mapped into the
; processes memoryspace.
;
align 4096
__get_exit_vector:
    push rax
    ; Move syscall ID into rax
    mov rax, SYSID_GET_EXITVEC

    push rbx
    push rdx
    push rdi
    push rsi
    push r8

    ; Move the address of __exit_vec into syscall arg0
    lea rbx, [rel __exit_vec]

    ; Make the call
    syscall

    pop r8
    pop rsi
    pop rdi
    pop rdx
    pop rbx

    ; Restore our rax
    pop rax
    ; Get out
    retq
___app_trampoline:
    ; Save our shit
    push rdi
    push rsi
    push rdx
    push rcx

    ; Move the pointers for the inits into registers
    lea rdi, [rel __app_entrypoint]
    lea rsi, [rel __lib_entrypoints]
    lea rdx, [rel __lib_entrycount]

    ; Now move the actual values at those addresses into the pointers
    mov rdi, [rdi]
    mov rsi, [rsi]
    mov rdx, [rdx]

    mov rcx, rdx

__init_lib:
    ; Check if we have any libraries
    cmp rcx, 0
    ; No libraries to init at this point, let's go to the main entry yay
    je __libinit_end
    ; Preserve the sys-v registers
    push rcx
    push rdi
    push rsi
    push rdx

    ; Save our buddy
    mov rcx, rsi

    ; Zero them for library execution
    mov rdi, 0
    mov rsi, 0
    mov rdx, 0

    ; Call the library entry
    call [rcx]

    ; Restore the registers
    pop rdx
    pop rsi
    pop rdi
    pop rcx

    ; Check if the library call failed
    cmp rax, 0
    jne __exit_error

    ; Move to the next library entry
    add rsi, 8

    ; Decrement our counter and itterate
    dec rcx
    jmp __init_lib

__libinit_end:
    ; Move the app entry to rax for safekeeping
    mov rax, rdi

    ; Restore the registers which contain the actual
    ; parameters for our userspace application
    pop rcx
    pop rdx
    pop rsi
    pop rdi

    ; Call the app entry
    call rax

__exit_error:
    ; Prepare the SYSID_EXIT syscall

    ; Move the app exit code (from rax) onto the stack
    push rax
    mov rbx, rax

    ; Make a systemcall to get the exitvector
    call __get_exit_vector

    ; If rax (error_t) == 0, we succesfully got the vector.
    ; Otherwise we need to skip calling
    cmp rax, 0
    jne __skip_exitvec

    ;
    ; Walk the exitvector and call all fuckers
    ;

    ; Load the pointer [rdi] = (exitvec_t**)
    lea rdi, [rel __exit_vec]
    ; [rdi] = (exitvec_t*)
    mov rdi, [rdi]

    ; Move the vector length (rdi[0]) into our counter
    mov rcx, [rdi]
    ; Move the pointer to the actual vector into rax 
    mov rax, rdi
    add rax, 8

__call_vec:
    ; Check if we've reached the bottom
    cmp rcx, 0
    je __skip_exitvec

    ; Preserve our vector pointer
    push rax

    ; Quick check to see if we're not doing bullshit
    mov rdi, [rax]
    cmp rdi, 0

    ; Alg, we can just do the call
    jne ___continue_exit_call

    ; Clear this stack entry
    pop rax
    ; Dip
    jmp __skip_exitvec

___continue_exit_call:
    ; Preserve the sys-v registers
    push rcx
    push rdi
    push rsi
    push rdx

    ; Zero sys-v regs for library execution
    mov rdi, 0
    mov rsi, 0
    mov rdx, 0
    mov rcx, 0

    ; Call the library entry
    call [rax]

    ; Restore the registers
    pop rdx
    pop rsi
    pop rdi
    pop rcx

    ; Check if the library call failed
    cmp rax, 0
    jne __exit_error

    ; Retrieve our vector pointer
    pop rax

    ; Move to the next library exit 
    add rax, 8

    ; Decrement and go next
    dec rcx
    jmp __call_vec
__skip_exitvec:
    ; Pop the app return value back into our arg0 register for a syscall
    pop rbx
    ; Move one into rax to signal the EXIT syscall
    mov rax, 1
    ; Call the syscall (FIXME: What if we're on a 'legacy' system which does not support the syscall instruction?)
    syscall

    ; Should not be reached...
    ;ret
__exit_loop:
    jmp __exit_loop;

__quick_exit:
    mov rbx, 0
    mov rax, 1

    syscall

    jmp __exit_loop

; (rdi=(DYNAPP_ENTRY_t) app_entry, rsi=(DYNLIB_ENTRY_t*) lib_entries, rdx=(size_t) lib_entry_count)
__app_entrypoint:
    dq 0
__lib_entrypoints:
    dq 0
__lib_entrycount:
    dq 0
__exit_vec:
    dq 0
